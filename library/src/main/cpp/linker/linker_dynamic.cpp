//
// Created by beich on 2024/5/25.
//
#include <jni.h>

#include "fake_linker.h"
#include "linker_macros.h"
#include "macros.h"


C_API JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    async_safe_fatal("JNI environment error");
  }
  jint code = init_fakelinker(env, FakeLinkerMode::kFMFully);
  if (code != 0) {
    LOGE("init fakelinker result: %d", code);
  }
  return JNI_VERSION_1_6;
}