#include "L06_audio_video_sync.h"
#define JZPLAYER "jzplayer"
AVStream *audio_steam;
AVStream *video_steam;
AVCodecContext *audio_ctx;
struct SwsContext *sws_ctx;

static SDL_AudioDeviceID audio_dev;
FrameQueue audio_q;
FrameQueue video_q;

unsigned int audio_buf_size; /* in bytes */
int audio_buf_index; /* in bytes */

AVFrame *avFrameRGBA;
void jzplayer();

//void *read_thread(char *url);
void read_thread(char *url, int audio_open());