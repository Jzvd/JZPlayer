#include "L06_audio_video_sync.h"
#include "jzplayer.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"

void read_thread(char *url, int audio_open()) {
    //初始化一些东西
    if (frame_queue_init(&audio_q, NULL, SAMPLE_QUEUE_SIZE, 1) < 0) {
        printf("frame_queue_init audio_q ERROR");
        return;
    }
    if (frame_queue_init(&video_q, NULL, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0) {
        printf("frame_queue_init video_q ERROR");
        return;
    }
    //视频声音的参数是，频率：44100，声道数：2，格式：AV_SAMPLE_FMT_FLTP
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    int ret = avformat_open_input(&fmt_ctx, url, NULL, NULL);
    if (ret < 0) {
        printf("avformat_open_input ERROR");
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);//没有这行不行，sws_getContext这个函数闪退。
    int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    audio_steam = fmt_ctx->streams[audio_index];
    video_steam = fmt_ctx->streams[video_index];
    audio_ctx = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(audio_ctx, fmt_ctx->streams[audio_index]->codecpar);
    const AVCodec *audio_codec = avcodec_find_decoder(audio_ctx->codec_id);
    if ((ret = avcodec_open2(audio_ctx, audio_codec, NULL)) < 0) {
        printf("avcodec_open2 ERROR");
    }
    AVCodecContext *video_ctx = avcodec_alloc_context3(NULL);
    ret = avcodec_parameters_to_context(video_ctx, fmt_ctx->streams[video_index]->codecpar);
    const AVCodec *video_codec = avcodec_find_decoder(video_ctx->codec_id);
    if ((ret = avcodec_open2(video_ctx, video_codec, NULL)) < 0) {
        printf("avcodec_open2 ERROR");
    }


    avFrameRGBA = av_frame_alloc();
    int width = video_ctx->width;
    int height = video_ctx->height;

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height,
                                            1);//2073600,align123在效果上没看出区别，数字也一样。2088960=4*544*960
    printf("numBytes - %d - %lu\n", numBytes, sizeof(uint8_t));
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(avFrameRGBA->data, avFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA, width,
                         height, 1);
    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    sws_ctx = sws_getContext(width, height, video_ctx->pix_fmt,//这个fmt是啥？
                             width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL,
                             NULL, NULL);//如果高宽改变变小了。是否影响解码时间呢？




    /* prepare audio output */
    if ((ret = audio_open(NULL)) < 0) {
        printf("audio_open ERROR");
    }
    audio_buf_size = 0;
    audio_buf_index = 0;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frm = av_frame_alloc();
    Frame *af;
    for (;;) {
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            printf("return\n");
            break;
        }
        if (pkt->stream_index == audio_index) {
            avcodec_send_packet(audio_ctx, pkt);
            avcodec_receive_frame(audio_ctx, frm);
            af = frame_queue_peek_writable(&audio_q);
            af->serial = 1;//is->auddec.pkt_serial;//这个不能删除，很重要啊，没有声音直接。
            av_frame_move_ref(af->frame, frm);
            frame_queue_push(&audio_q);
            printf("put audio %d\n", audio_q.size);
        } else if (pkt->stream_index == video_index) {
            avcodec_send_packet(video_ctx, pkt);
            avcodec_receive_frame(video_ctx, frm);
            //放到列表里。
            printf("video frame_queue_peek_writable %d\n", video_q.size);
            af = frame_queue_peek_writable(&video_q);
            av_frame_move_ref(af->frame, frm);
            frame_queue_push(&video_q);
            printf("put video %d\n", video_q.size);
        }
        av_packet_unref(pkt);
        av_frame_unref(frm);
    }
    printf("read thread over\n");
}


void jzplayer() {

}