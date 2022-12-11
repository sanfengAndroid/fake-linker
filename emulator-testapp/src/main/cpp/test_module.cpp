#include <android/log.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <jni.h>
#include <unistd.h>
#ifdef OPEN_HOOK
#include "Dobby/dobby.h"
#endif

#ifdef LOGW
#undef LOGW
#endif
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "Fakelinker_Test", ##__VA_ARGS__)
int g_log_level = 2;
extern "C" JNIEXPORT int test_x86_arm_hook_before(int a) {
  LOGW("test x86 arm hook before input: %d", a);
  return a + 1;
}

extern "C" JNIEXPORT int test_x86_arm_hook_after(int a) {
  LOGW("test x86 arm hook after input: %d", a);
  return a - 1;
}

#ifdef OPEN_HOOK

static int (*orig_test_before_hook)(int) = nullptr;
static int (*orig_test_after_hook)(int) = nullptr;

int test_before_hook_replace(int a) {
  LOGW("test houdini arm before hook success");
  return orig_test_before_hook(a + 1);
}

int test_after_hook_replace(int a) {
  LOGW("test houdini arm after hook success");
  return orig_test_after_hook(a - 1);
}

extern "C" JNIEXPORT void JNICALL Java_com_sanfengandroid_testapp_MainActivity_beforeHookSymbol(JNIEnv *env,
                                                                                                jobject thiz,
                                                                                                jlong addr) {
  if (addr > 0) {
    DobbyHook(reinterpret_cast<void *>(addr), reinterpret_cast<dobby_dummy_func_t>(&test_before_hook_replace),
              reinterpret_cast<void (**)()>(&orig_test_before_hook));
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_sanfengandroid_testapp_MainActivity_afterHookSymbol(JNIEnv *env,
                                                                                               jobject thiz,
                                                                                               jlong addr) {
  if (addr > 0) {
    DobbyHook(reinterpret_cast<void *>(addr), reinterpret_cast<dobby_dummy_func_t>(&test_after_hook_replace),
              reinterpret_cast<void (**)()>(&orig_test_after_hook));
  }
}

#endif

extern "C" JNIEXPORT void JNICALL
Java_com_sanfengandroid_testapp_MainActivity_testArmSymbolForFakeinkerHook(JNIEnv *env, jobject thiz) {
  void *libc = dlopen("libm.so", RTLD_NOW);
  LOGW("found libc module: %p, hook function address: %p", libc, test_x86_arm_hook_before);
  if (libc) {
    void *sym = dlsym(libc, "dlopen");
    LOGW("found dlopen: %p, current relocation: %p", sym, dlopen);
    LOGW("getuid: %d", getgid());
    access("/data/local", 0);
    unlink("/data/local/adg");
  }
  bool hooked = close(-1) == -5;
  LOGW("test emulator arm symbol for fakelinker hook result: %s", hooked ? "true" : "false");
}

extern "C" JNIEXPORT void JNICALL Java_com_sanfengandroid_testapp_MainActivity_testAfterHook(JNIEnv *env,
                                                                                             jobject thiz) {
  int a = 10;
  a = test_x86_arm_hook_after(a);
  LOGW("test after hook output: %d, hook success: %s", a, a == 8 ? "true" : "false");
}
extern "C" JNIEXPORT void JNICALL Java_com_sanfengandroid_testapp_MainActivity_testBeforeHook(JNIEnv *env,
                                                                                              jobject thiz) {
  int a = 10;
  a = test_x86_arm_hook_before(a);
  LOGW("test before hook output: %d, hook success: %s", a, a == 12 ? "true" : "false");
}
