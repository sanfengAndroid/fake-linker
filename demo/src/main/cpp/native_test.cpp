//
// Created by beichen on 2025/3/12.
//
#include <android/log.h>
#include <jni.h>
#include <unistd.h>

#define LOG_TAG           "NativeTest"

#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, format, ##__VA_ARGS__);

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    LOGE("JNI environment error");
    return JNI_EVERSION;
  }

  if (access("/test/hook/path", F_OK) != 0) {
    LOGE("hook module error");
    return JNI_ERR;
  }
  return JNI_VERSION_1_6;
}