#ifndef FFMPEGCMAKE_HAHA_H
#define FFMPEGCMAKE_HAHA_H

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

static void frame_queue_unref_item(Frame *vp) {
    av_frame_unref(vp->frame);
}

static int frame_queue_init(FrameQueue *f, int max_size, int keep_last) {
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (pthread_mutex_init(&f->mutex, NULL)) {
        av_log(NULL, AV_LOG_FATAL, "pthread_mutex_init(): %s\n", strerror(errno));
        return AVERROR(ENOMEM);
    }
    if (pthread_cond_init(&f->cond, NULL)) {
        av_log(NULL, AV_LOG_FATAL, "pthread_cond_init(): %s\n", strerror(errno));
        return AVERROR(ENOMEM);
    }
//    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    return 0;
}

static void frame_queue_destory(FrameQueue *f) {
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
    }
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->cond);
}

static void frame_queue_signal(FrameQueue *f) {
    pthread_mutex_lock(&f->mutex);
    pthread_cond_signal(&f->cond);
    pthread_mutex_unlock(&f->mutex);
}

static Frame *frame_queue_peek(FrameQueue *f) {
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static Frame *frame_queue_peek_next(FrameQueue *f) {
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static Frame *frame_queue_peek_last(FrameQueue *f) {
    return &f->queue[f->rindex];
}

static Frame *frame_queue_peek_writable(FrameQueue *f) {
    /* wait until we have space to put a new frame */
    pthread_mutex_lock(&f->mutex);
    while (f->size >= f->max_size
// && !f->pktq->abort_request
            ) {
        pthread_cond_wait(&f->cond, &f->mutex);
    }
    pthread_mutex_unlock(&f->mutex);

// if (f->pktq->abort_request)
// return NULL;
    return &f->queue[f->windex];
}

static Frame *frame_queue_peek_readable(FrameQueue *f) {
    /* wait until we have a readable a new frame */
    pthread_mutex_lock(&f->mutex);
    while (f->size - f->rindex_shown <= 0
// && !f->pktq->abort_request
            ) {
        pthread_cond_wait(&f->cond, &f->mutex);
    }
    pthread_mutex_unlock(&f->mutex);

// if (f->pktq->abort_request)
// return NULL;
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];

}

static void frame_queue_push(FrameQueue *f) {
    if (++f->windex == f->max_size)
        f->windex = 0;
    pthread_mutex_lock(&f->mutex);
    f->size++;
    pthread_cond_signal(&f->cond);
    pthread_mutex_unlock(&f->mutex);
}

static void frame_queue_next(FrameQueue *f) {
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    pthread_mutex_lock(&f->mutex);
    f->size--;
    pthread_cond_signal(&f->cond);
    pthread_mutex_unlock(&f->mutex);
}

/* return the number of undisplayed frames in the queue */
static int frame_queue_nb_remaining(FrameQueue *f) {
    return f->size - f->rindex_shown;
}

#endif //FFMPEGCMAKE_HAHA_H

