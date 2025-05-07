//
// Created by WrBug on 2018/3/23.
//
#include <android/api-level.h>
#include <android/log.h>
#include <inttypes.h>
#include <string>

#include <fakelinker/fake_linker.h>
#include <fakelinker/maps_util.h>
#include <fakelinker/scoped_utf_chars.h>

#define TAG "FakeLinker_Test"

#undef LOGE
#undef LOGI
#undef LOGD
#undef LOGW

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

const FakeLinker *remote;

static int api;
static SoinfoPtr thiz;
int g_log_level = 2;
static bool loaded = false;

static int get_api_level() {
  if (api > 0) {
    return api;
  }
  api = android_get_device_api_level();
  return api;
}

static int failed_num = 0;
static int success_num = 0;

static void test_output(bool success, const char *msg) {
  if (success) {
    LOGI("test %s pass.", msg);
    success_num++;
  } else {
    LOGE("test %s failed!", msg);
    failed_num++;
  }
}

C_API API_PUBLIC void *dlopen(const char *filename, int flags) {
  void *br = __builtin_return_address(0);
  void *res = remote->call_dlopen_inside(filename, flags, br, nullptr);
  LOGW("dlopen name: %s, flags: %d, result: %p, br: %p", filename, flags, res, br);
  return res;
}

C_API API_PUBLIC int close(int fd) {
  if (fd == -1) {
    test_output(true, "hook arm close function success");
    return -5;
  }
  return 0;
}

C_API API_PUBLIC void *dlsym(void *handle, const char *symbol) {
  std::string symbolName = symbol;
  void *addr;
  if (symbolName == "dlopen") {
    addr = reinterpret_cast<void *>(&dlopen);
    LOGI("hook dlopen address: %p", addr);
    return addr;
  } else if (symbolName == "dlsym") {
    addr = reinterpret_cast<void *>(&dlsym);
    LOGE("hook dlsym address: %p", addr);
    return addr;
  } else if (symbolName == "close") {
    addr = reinterpret_cast<void *>(&close);
    LOGE("hook close address: %p", addr);
    return addr;
  }
  void *br = __builtin_return_address(0);
  addr = remote->get_dlsym_inside_func_ptr()(handle, symbol, br);
  LOGW("dlsym handle: %p,  name: %s, result: %p, br: %p", handle, symbol, addr, br);
  return addr;
}

C_API API_PUBLIC void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker) {
  remote = fake_linker;
  api = get_api_level();
  thiz = fake_soinfo;
  loaded = remote->is_init_success();
  if (!loaded) {
    LOGE("fakelinker module init failed");
    return;
  }
  LOGE("init hook module success");
  remote->add_relocation_blacklist("close");
  remote->soinfo_add_to_global(thiz);
}

C_API JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *unused) {
  LOGI("load fakelinker module");
  return JNI_VERSION_1_6;
}
C_API JNIEXPORT void JNICALL Java_com_sanfengandroid_testapp_MainActivity_relocateLibrary(JNIEnv *env, jobject,
                                                                                          jstring library) {
  if (!library || !loaded) {
    return;
  }
  ScopedUtfChars name(env, library);
  LOGW("relocate library: %s, result: %d", name.c_str(), remote->call_manual_relocation_by_name(thiz, name.c_str()));
}
C_API JNIEXPORT jlong JNICALL Java_com_sanfengandroid_testapp_MainActivity_findModuleBeforeAddress(JNIEnv *env,
                                                                                                   jobject _unused) {
  if (!loaded) {
    return 0;
  }
  LOGI("find hook module test_x86_arm_hook_before address");
  Address addr =
    remote->find_library_symbol("libtest_module.so", "test_x86_arm_hook_before", FindSymbolType::kExported);
  LOGI("find result: %" PRIx64, addr);
  return addr;
}
C_API JNIEXPORT jlong JNICALL Java_com_sanfengandroid_testapp_MainActivity_findModuleAfterAddress(JNIEnv *env,
                                                                                                  jobject _unused) {
  if (!loaded) {
    return 0;
  }
  return remote->find_library_symbol("libtest_module.so", "test_x86_arm_hook_after", FindSymbolType::kExported);
}