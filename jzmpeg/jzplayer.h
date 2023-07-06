#ifndef JZPLAYER_JZPLAYER_H
#define JZPLAYER_JZPLAYER_H

#include "L06_audio_video_sync.h"

#define JZPLAYER "jzplayer"
const char *shor_video_url = "http://out-20170815011809382-tto5l35rgt.oss-cn-shanghai.aliyuncs.com/video/25ae1b1c-1767b2a5e44-0007-1823-c86-de200.mp4?Expires=1639071363&OSSAccessKeyId=LTAIwkKSLcUfI2u4";


static AVStream *audio_steam;
static AVStream *video_steam;
static AVCodecContext *audio_ctx;
static struct SwsContext *sws_ctx;

static FrameQueue audio_q;
FrameQueue video_q;

static unsigned int audio_buf_size; /* in bytes */
static int audio_buf_index; /* in bytes */

static AVFrame *avFrameRGBA;

void jzplayer();

//void *read_thread(char *url);
void read_thread(const char *url, int audio_open());

#endif //JZPLAYER_JZPLAYER_H