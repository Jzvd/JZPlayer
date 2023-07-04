#include "jzplayer.h"
#include <stdio.h>
#include "SDL2/SDL.h"
#include "pthread.h"


const char *video_url = "http://out-20170815011809382-tto5l35rgt.oss-cn-shanghai.aliyuncs.com/video/25ae1b1c-1767b2a5e44-0007-1823-c86-de200.mp4?Expires=1639071363&OSSAccessKeyId=LTAIwkKSLcUfI2u4";
SDL_Window *screen = NULL;
SDL_Texture *texture = NULL;
SDL_Renderer *renderer = NULL;

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30
#define AV_SYNC_THRESHOLD 0.01
static SDL_AudioDeviceID audio_dev;
enum AVSampleFormat fmt;
AVChannelLayout ch_layout;//这两个都是hw的参数。
int freq;
struct SwrContext *swr_ctx;

int frame_size;//4

uint8_t *audio_buf;
uint8_t *audio_buf1;
double audio_clock = 0.0;

SDL_Event event;

static int audio_decode_frame() {
    int data_size/*8192*/, resampled_data_size;//这两个有什么区别。一个是没解码的，一个是解码后的。
    Frame *af;

    if (!(af = frame_queue_peek_readable(&audio_q)))
        return -1;
    frame_queue_next(&audio_q);
    printf("get audio\n");
    data_size = av_samples_get_buffer_size(NULL, af->frame->ch_layout.nb_channels,
                                           af->frame->nb_samples,
                                           af->frame->format, 1);//8192=2*4*1024    4：float类型占用的字节数

    if (af->frame->format != fmt ||
        av_channel_layout_compare(&af->frame->ch_layout, &ch_layout) ||
        af->frame->sample_rate != freq || !swr_ctx) {//如果音频的参数不一致，就需要重新初始化swr_ctx。
        swr_free(&swr_ctx);
        swr_alloc_set_opts2(&swr_ctx,
                            &ch_layout, fmt, freq,
                            &af->frame->ch_layout, af->frame->format, af->frame->sample_rate,
                            0, NULL);//这段其实特别简单，就是看着复杂，其实非常易懂。
        if (!swr_ctx || swr_init(swr_ctx) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                   af->frame->sample_rate, av_get_sample_fmt_name(af->frame->format), af->frame->ch_layout.nb_channels,
                   freq, av_get_sample_fmt_name(fmt), ch_layout.nb_channels);
            swr_free(&swr_ctx);
            return -1;
        }
    }

    //这段代码不复杂，swr_convert()给这个函数准备数据，这段就是这一个函数。
    if (swr_ctx) {
        //重采样后音频数据缓冲区的采样数。
        int out_count =
                (int64_t) af->frame->nb_samples * freq / af->frame->sample_rate + 64;//1280=1024+256 //256-100-50都可以。没搞懂
        //重采样后音频数据缓冲区的大小（单位：字节）
        int out_size = av_samples_get_buffer_size(NULL, ch_layout.nb_channels, out_count,
                                                  fmt, 0);//5120
        if (out_size < 0) {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        unsigned int audio_buf1_size;
        av_fast_malloc(&audio_buf1, &audio_buf1_size, out_size);//out_size=4352 audio_buf1_size=4656or24576
//        printf("audio_buf1_size=%d out_size=%d\n", audio_buf1_size, out_size);
        if (!audio_buf1)
            return AVERROR(ENOMEM);
        int len2;//1024 是重采样后单个通道的采样数。
        len2 = swr_convert(swr_ctx, &audio_buf1, out_count,
                           (const uint8_t **) af->frame->extended_data, af->frame->nb_samples);
        if (len2 < 0) {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count) {
            av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
            if (swr_init(swr_ctx) < 0)
                swr_free(&swr_ctx);
        }
        audio_buf = audio_buf1;
//        printf("resampled_data_size %d data_size %d\n", resampled_data_size, data_size);
        resampled_data_size = len2 * ch_layout.nb_channels * av_get_bytes_per_sample(fmt);//4096 单个通道采样数*通道个数*每个采样的字节数
    } else {
        audio_buf = af->frame->data[0];
        resampled_data_size = data_size;
    }

    int n = 2 * ch_layout.nb_channels;
    audio_clock += (double) resampled_data_size / (double) (n * audio_ctx->sample_rate);
    printf("audio_clock %f %d\n", audio_clock, audio_q.size);
//    audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;这个pts在decode frame之后才会取得。
//    printf("frame pts %d\n", af->frame->pts);
    return resampled_data_size;
}

static void sdl_audio_callback(void *opaque, Uint8 *stream, int len) {//len=8192 需要两次解码的数据才能填满。一次4096，就这样。
//    printf("len1=%d\n", len);
    int audio_size, len1;
    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            audio_size = audio_decode_frame();
//            printf("读取出来%d个字节\n", audio_size);
            if (audio_size < 0) {
                /* if error, just output silence */
                audio_buf = NULL;
                audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / frame_size * frame_size;
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;

        }
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len)
            len1 = len;
        if (audio_buf) {
//            printf("memcpy%d个字节，index=%d\n", len1, audio_buf_index);
            memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
//            break;
        } else {
            memset(stream, 0, len1);
        }
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
    /* Let's assume the audio driver that is used by SDL has two periods. */
}

//这个函数超级简单：设置音频参数，把实际的音频参数传出来。
int audio_open() {
    SDL_AudioSpec wanted_spec, spec;
    wanted_spec.channels = audio_ctx->ch_layout.nb_channels;
    wanted_spec.freq = audio_ctx->sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        return -1;
    }

    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE,
                                2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));//2048
    wanted_spec.callback = sdl_audio_callback;
//    wanted_spec.userdata = opaque;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec,
                                    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
           wanted_spec.channels, wanted_spec.freq, SDL_GetError());
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio fmt %d is not supported!\n", spec.format);
        return -1;
    }
    //这里给hw设置参数，hw的参数是代码设置的。
    fmt = AV_SAMPLE_FMT_S16;
    freq = spec.freq;
    if (av_channel_layout_copy(&ch_layout, &audio_ctx->ch_layout) < 0)//wanted_channel_layout是视频的声道布局
        return -1;
    frame_size = av_samples_get_buffer_size(NULL, audio_ctx->ch_layout.nb_channels, 1,
                                            fmt, 1);
    if (frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        return -1;
    }
    SDL_PauseAudioDevice(audio_dev, 0);
    return spec.size;//8192
}

void *read_thread1() {
    read_thread(video_url,
                audio_open);
}

double last_pts = 0;
double frame_timer = 0;

double get_audio_clock() {//如果直接返回audio_clock不计算细节会怎么样。
    double pts = audio_clock;

    return pts;
}

double get_delay_time(AVFrame *frm) {
    double delay, sync_threshold, the_clock, diff, actual_delay;
    double pts = frm->pts * av_q2d(video_steam->time_base);
//    printf("pts=%f frm.pts=%f\n", pts, frm->pts);
    if (pts < 0) {
        pts = 0;
    }
    delay = pts - last_pts;
    last_pts = pts;

    sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
    the_clock = get_audio_clock();
    diff = pts - the_clock;//diff>0的话：视频比音频快-需要减速视频播放-延长delay，diff<0的话：视频比音频落后，需要加速视频播放-抛弃frame。
    printf("diff=%f\n", diff);
    if (diff < 0) {//这里应该视频比音频落后应该丢掉这个帧
//        diff = -1;
//        /** 这里是这个项目的重点，也就是说用这种形式是无法正常播放音视频。 因为数据没有缓存，音频和视频的数据在一条线上
//            即便这一帧图像丢弃了，下一帧图像也追不上来，被音频帧卡住了，音频帧被拿走之后下一个视频帧才能漏出来。所以必须得用列表缓存起来。
//            这就比较冗余了，ffmpeg内部读取出数据来有一个缓存列表，这里还要有个缓存列表。**/
        diff = 0;
    } else {
        diff = diff * 2;
    }
    return diff;//40000 是40ms
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        av_log(NULL, AV_LOG_FATAL, "Could not initialize SDL - %s\n", SDL_GetError());
        av_log(NULL, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        exit(1);
    }
    int width = 540;
    int height = 960;
    screen = SDL_CreateWindow("My Game Window",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              width, height, 0);
    if (!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    // Allocate a place to put our YUV image on that screen.
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                width, height);//SDL_PIXELFORMAT_RGB24不行
    pthread_t read_th;
    pthread_create(&read_th, NULL, read_thread1, NULL);  //创建线程写数据

    for (;;) {
        Frame *af;
        printf("pick video %d\n", video_q.size);
        if (!(af = frame_queue_peek_readable(&video_q))) {
            printf("frame_queue_peek_readable ERROR");
        }
        frame_queue_next(&video_q);
        printf("get video\n");
        double aucture_delay = get_delay_time(af->frame);
//        if (aucture_delay < 0) {
//            continue;//图像不能丢，如果丢了会显得有点卡。
//        } else {
        av_usleep(aucture_delay * 1000000.0);
//        }
        SDL_PollEvent(&event);
        switch (event.type) {
            case SDL_QUIT:
                printf("SDL_QUIT\n");
                SDL_Quit();
                exit(0);
            default:
                break;
        }
//        int events = 0;//不行不用sdl，用macos的library。
//        while (!(events = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))) {//这块鼠标移动才能继续，就是说必须得有有效的事件才能继续
        //就在这同步视频。非常高调。
        sws_scale(sws_ctx, (uint8_t const *const *) af->frame->data,
                  af->frame->linesize, 0, height,
                  avFrameRGBA->data, avFrameRGBA->linesize);

        // 使用 RGB 数据更新纹理
        SDL_UpdateTexture(texture, NULL, avFrameRGBA->data[0], avFrameRGBA->linesize[0]);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);//ffplay中这个函数没走到。这里没他还不行，有点复杂。
        SDL_RenderPresent(renderer);//这个是重点。

//        }

    }
    printf("hello %s", JZPLAYER);
    return 0;
}
