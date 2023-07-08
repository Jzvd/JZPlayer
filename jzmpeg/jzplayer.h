#ifndef JZPLAYER_JZPLAYER_H
#define JZPLAYER_JZPLAYER_H

#include "pkt_frm_list.h"

#define JZPLAYER "jzplayer"

static const char *shor_video_url = "http://out-20170815011809382-tto5l35rgt.oss-cn-shanghai.aliyuncs.com/video/25ae1b1c-1767b2a5e44-0007-1823-c86-de200.mp4?Expires=1639071363&OSSAccessKeyId=LTAIwkKSLcUfI2u4";


//void *read_thread(char *url);
void read_thread(const char *url, int audio_open());

#endif //JZPLAYER_JZPLAYER_H