#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "util.h"
#include "aaudio/AAudio.h"
#include "pthread.h"

#define AV_SYNC_THRESHOLD 0.01
#define SDL_AUDIO_MIN_BUFFER_SIZE 512


extern "C" {
#include "jzplayer.h"
#include <libavutil/avutil.h>

//extern AVStream *audio_steam;
//extern AVStream *video_steam;
//extern AVCodecContext *audio_ctx;
//extern struct SwsContext *sws_ctx;
//extern FrameQueue audio_q;
//extern FrameQueue video_q;
//extern unsigned int audio_buf_size; /* in bytes */
//extern int audio_buf_index; /* in bytes */
//extern AVFrame *avFrameRGBA;

double last_pts = 0;
double frame_timer = 0;
double audio_clock = 0.0;
static uint8_t *audio_buf;

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
static enum AVSampleFormat PLAY_FORMAT = AV_SAMPLE_FMT_S16;
static int PLAY_CHANNELS = 2;
static AVChannelLayout PLAY_ch_layout;//这两个都是hw的参数。
static int PLAY_SAMPLERATE = 44100;
static AAudioStream *stream_;
static int frame_size;//4
static struct SwrContext *swr_ctx;

static int audio_decode_frame() {
    Frame *af;
    if (!(af = frame_queue_peek_readable(&audio_q)))
        return -1;
    frame_queue_next(&audio_q);
    if (audio_buf == NULL) {
        audio_buf = (uint8_t *) malloc(
                av_get_bytes_per_sample(PLAY_FORMAT) * af->frame->nb_samples * PLAY_CHANNELS);
    }
    memset(audio_buf, 0,
           av_get_bytes_per_sample(PLAY_FORMAT) * af->frame->nb_samples * PLAY_CHANNELS);
    //1024 是重采样后单个通道的采样数。
    int len2 = swr_convert(swr_ctx, &audio_buf, af->frame->nb_samples,
                           (const uint8_t **) af->frame->extended_data, af->frame->nb_samples);
    if (len2 < 0) {
        LOGE("swr_convert() failed\n");
        return -1;
    }
    int resampled_data_size = len2 * PLAY_CHANNELS * av_get_bytes_per_sample(PLAY_FORMAT);
    int n = 2 * PLAY_CHANNELS;
    audio_clock += (double) resampled_data_size / (double) (n * audio_ctx->sample_rate);
    printf("audio_clock %f %d\n", audio_clock, audio_q.size);
    return resampled_data_size;//4096 单个通道采样数*通道个数*每个采样的字节数
}
static aaudio_data_callback_result_t
myAAudioStreamDataCallbackPlay_I16(AAudioStream *stream, void *userData, void *audioData,
                                   int32_t numFrames) {
    int bytes_per_frame = av_get_bytes_per_sample(PLAY_FORMAT) * PLAY_CHANNELS;
    int len = numFrames * bytes_per_frame;//本次回调需要多少个字节
    LOGE("进入回调，总共 %d\n", len);
    int audio_size, len1;//len1是本次要写多少个字节；audio_size是本次转码了多少数据
    while (len > 0) {
        if (audio_buf_index >= audio_buf_size) {
            audio_size = audio_decode_frame();
            LOGW("读取出来%d个字节\n", audio_size);
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
        if (len1 > len) {
            LOGV("本次还剩余字节数大于需要的字节数 %d > %d\n", len1, len);
            len1 = len;
        }
        if (audio_buf) {
            LOGI("memcpy%d个字节，index=%d\n", len1, audio_buf_index);
            memcpy(audioData, audio_buf + audio_buf_index, len1);
        } else {
            LOGI("静默 %d", len1);
            memset(audioData, 0, len1);
        }
        LOGI("while 本次小循环写入了%d个字节，还剩余%d个字节如果>0需要再次转码  此时的index是%d。\n",
             len1, len - len1, audio_buf_index + len1);
        len -= len1;
        char *int_ptr = static_cast<char *>(audioData);
        int_ptr += len1;// / av_get_bytes_per_sample(PLAY_FORMAT);
        audio_buf_index += len1;
    }
    LOGE("退出回调\n");
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
int audio_open() {
    AAudioStreamBuilder *streamBuilder;
    AAudio_createStreamBuilder(&streamBuilder);
    AAudioStreamBuilder_setDeviceId(streamBuilder, AAUDIO_UNSPECIFIED);
    AAudioStreamBuilder_setFormat(streamBuilder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(streamBuilder, PLAY_CHANNELS);
    AAudioStreamBuilder_setSampleRate(streamBuilder, PLAY_SAMPLERATE);
    AAudioStreamBuilder_setSharingMode(streamBuilder, AAUDIO_SHARING_MODE_EXCLUSIVE);
    AAudioStreamBuilder_setPerformanceMode(streamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setDirection(streamBuilder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setDataCallback(streamBuilder, myAAudioStreamDataCallbackPlay_I16, NULL);
    // Opens the stream.
    aaudio_result_t result = AAudioStreamBuilder_openStream(streamBuilder, &stream_);
    if (result != AAUDIO_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "AudioEngine", "Error opening stream %s",
                            AAudio_convertResultToText(result));
    }
    AAudioStreamBuilder_delete(streamBuilder);
    frame_size = av_samples_get_buffer_size(NULL, PLAY_CHANNELS, 1,
                                            PLAY_FORMAT, 1);
    return 0;
}
void *read_thread1() {
    read_thread(shor_video_url,
                audio_open);
    return NULL;
}
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_jzvd_jzplayer_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello C++ ";
    hello.append(JZPLAYER);
    hello.append(av_version_info());
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_org_jzvd_jzplayer_MainActivity_start(JNIEnv *env, jobject thiz, jobject surface) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    int width = 540;
    int height = 960;

    ANativeWindow_setBuffersGeometry(window, width, height,
                                     WINDOW_FORMAT_RGBA_8888);//有这行代码会铺满屏幕，没有它就按像素了吧。

    pthread_t read_th;
    pthread_create(&read_th, NULL,
                   reinterpret_cast<void *(*)(void *)>(read_thread1), NULL);  //创建线程写数据

    ANativeWindow_Buffer windowBuffer;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frm = av_frame_alloc();

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
        LOGE("sleep before\n");
        av_usleep(aucture_delay * 1000000.0);
        LOGE("sleep after %f\n", aucture_delay);
        ANativeWindow_lock(window, &windowBuffer, 0);//放在其他地方会怎样呢。
        sws_scale(sws_ctx, (uint8_t const *const *) af->frame->data,
                  af->frame->linesize, 0, height,
                  avFrameRGBA->data, avFrameRGBA->linesize);

        // 获取stride
        uint8_t *dst = (uint8_t *) windowBuffer.bits;
        int dstStride = windowBuffer.stride * 4;
        uint8_t *src = (avFrameRGBA->data[0]);
        int srcStride = avFrameRGBA->linesize[0];//det2304 -src2160 全一样
        for (int h = 0; h < height; h++) {
            memcpy(dst + h * dstStride,
                   src +
                   h * srcStride/**+5*h图像斜歪了 +5 这里就不是位置的变化，是颜色的变化为啥**/,
                   srcStride);
        }
        ANativeWindow_unlockAndPost(window);
    }

}