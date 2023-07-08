//
// Created by lipangit on 2023/7/8.
//
#ifndef FFMPEGCMAKE_PKT_FRM_LIST_H
#define FFMPEGCMAKE_PKT_FRM_LIST_H

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

//#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/fifo.h"
#include "libswresample/swresample.h"

#include <pthread.h>
#include <libavcodec/avcodec.h>


#define VIDEO_PICTURE_QUEUE_SIZE 30
#define SAMPLE_QUEUE_SIZE 90
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, VIDEO_PICTURE_QUEUE_SIZE)

typedef struct Frame {
    AVFrame *frame;
    int serial;
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
} Frame;
//typedef struct PacketQueue {
//    AVFifo *pkt_list;
//    int nb_packets;
//    int size;
//    int64_t duration;
//    int abort_request;
//    int serial;
//    pthread_mutex_t mutex;
//    pthread_cond_t cond;
//} PacketQueue;
typedef struct FrameQueue {
    Frame queue[FRAME_QUEUE_SIZE];
    int rindex;
    int windex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
//    PacketQueue *pktq;
} FrameQueue;


void frame_queue_unref_item(Frame *vp);

int frame_queue_init(FrameQueue *f, int max_size, int keep_last);

void frame_queue_destory(FrameQueue *f);

void frame_queue_signal(FrameQueue *f);

Frame *frame_queue_peek(FrameQueue *f);

Frame *frame_queue_peek_next(FrameQueue *f);

Frame *frame_queue_peek_last(FrameQueue *f);

Frame *frame_queue_peek_writable(FrameQueue *f);

Frame *frame_queue_peek_readable(FrameQueue *f);

void frame_queue_push(FrameQueue *f);

void frame_queue_next(FrameQueue *f);

int frame_queue_nb_remaining(FrameQueue *f);

#endif