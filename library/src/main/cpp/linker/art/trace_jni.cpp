//
// Created by beichen on 2025/4/25.
//

#include <fakelinker/trace_jni.h>

namespace fakelinker {
namespace jni_trace {

JNINativeInterface backup_jni;
JNINativeInterface *org_jni = nullptr;
JNINativeInterface hook_jni;
void *monitor = nullptr;
void *callback = nullptr;
JNIEnv org_env{.functions = &backup_jni};

} // namespace jni_trace
} // namespace fakelinker
