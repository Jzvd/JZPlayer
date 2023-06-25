#include <jni.h>
#include <string>
#include "jzplayer.h"

extern "C" JNIEXPORT jstring JNICALL
Java_org_jzvd_jzplayer_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++ ";
    hello.append(JZPLAYER);
    return env->NewStringUTF(hello.c_str());
}