//
// Created by 李盼 on 2021/12/10.
//
// 这文件算c++还是c
// util相关的东西尽量写在h文件中
#ifndef JZFFMPEGDETAIL_UTIL_H
#define JZFFMPEGDETAIL_UTIL_H

#include <android/log.h>

#define LOG_TAG "FFMPEG_ANDROID"

#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define ALOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))


#endif //JZFFMPEGDETAIL_UTIL_H