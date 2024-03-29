#include <cstring>
#include <dlfcn.h>

#include <alog.h>
#include <fake_linker.h>
#include <linker_version.h>
#include <macros.h>
#include <maps_util.h>
#include <scoped_utf_chars.h>

#include "art/hook_jni_native_interface_impl.h"
#include "linker_globals.h"
#include "linker_symbol.h"
#include "linker_util.h"

int g_log_level = HOOK_LOG_LEVEL;
int g_version_code = MODULE_VERSION;
const char *g_version_name = MODULE_VERSION_NAME;
int android_api;
int android_api_T;
C_API FakeLinker g_fakelinker_export;
JNINativeInterface *original_functions;
bool init_success = false;

using FakeLinkerModulePtr = void (*)(JNIEnv *, SoinfoPtr, const FakeLinker *);


static void Init() {
  soinfo::Init();
  android_namespace_t::Init();
  fakelinker::linker_symbol.InitSymbolName();
  init_success = fakelinker::linker_symbol.LoadSymbol();
}

static void AndroidLog(std::string &str) {
  size_t size = str.length();
  size_t index = 0;
  while (size > index) {
    size_t next = index + 1500;
    char c = '\0';
    if (size > next) {
      c = str[next];
      str[next] = '\0';
    }

    LOGI("%s", str.c_str() + index);
    if (c != '\0') {
      str[next] = c;
    }
    index = next;
  }
}

static void InitHookModule(JNIEnv *env, void *module, const char *module_name) {
  FakeLinkerModulePtr module_ptr = reinterpret_cast<FakeLinkerModulePtr>(dlsym(module, FAKELINKER_LIB_INIT_NAME));
  if (module_ptr == nullptr) {
    LOGW("There is no 'fakelinker_module_init' symbol in the module: %s", module_name);
    return;
  }
  SoinfoPtr hook_ptr;
  if (android_api >= __ANDROID_API_N__) {
    hook_ptr = g_fakelinker_export.soinfo_find(kSTHandle, module, nullptr);
  } else {
    hook_ptr = module;
  }
  module_ptr(env, hook_ptr, &g_fakelinker_export);
}

static void FakeLinker_nativeOffset(JNIEnv *env, jclass clazz) { LOGW("This test native offset"); }

static int GetApiLevel() {
  char value[92] = {0};
  if (__system_property_get("ro.build.version.sdk", value) < 1)
    return -1;
  int api_level = atoi(value);
  return (api_level > 0) ? api_level : -1;
}

static jboolean FakeLinker_entrance(JNIEnv *env, jclass clazz, jstring hook_module_path) {
  LOGD("current api level: %d", android_api);
  if (!init_success) {
    LOGE("Failed to initialize fake-linker environment, the device is not supported");
    return JNI_FALSE;
  }
  if (hook_module_path == nullptr) {
    LOGE("Load Hook module is null");
    return JNI_FALSE;
  }
  ScopedUtfChars hook_module(env, hook_module_path);
  if (hook_module.size() == 0) {
    LOGE("Load null module error");
    return JNI_FALSE;
  }
  void *hookHandle = dlopen(hook_module.c_str(), RTLD_NOW);
  if (hookHandle == nullptr) {
    LOGE("load hook module failed: %s", dlerror());
    return JNI_FALSE;
  }
  InitHookModule(env, hookHandle, hook_module.c_str());
  return JNI_TRUE;
}

static void FakeLinker_setLogLevel(JNIEnv *env, jclass clazz, jint level) { g_log_level = level; }

static jint FakeLinker_relocationFilterSymbol(JNIEnv *env, jclass clazz, jstring symbol_name, jboolean add) {
  if (symbol_name == nullptr) {
    return kErrorParameterNull;
  }
  ScopedUtfChars symbol(env, symbol_name);
  if (add) {
    g_fakelinker_export.add_relocation_blacklist(symbol.c_str());
  } else {
    g_fakelinker_export.remove_relocation_blacklist(symbol.c_str());
  }
  return kErrorNo;
}

static void FakeLinker_removeAllRelocationFilterSymbol(JNIEnv *, jclass) {
  g_fakelinker_export.clear_relocation_blacklist();
}

#define NATIVE_METHOD(className, functionName, signature)                                                              \
  { #functionName, signature, (void *)(className##_##functionName) }

#ifndef NELEM
#define NELEM(x) ((int)(sizeof(x) / sizeof((x)[0])))
#endif

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(FakeLinker, nativeOffset, "()V"),
  NATIVE_METHOD(FakeLinker, entrance, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(FakeLinker, setLogLevel, "(I)V"),
  NATIVE_METHOD(FakeLinker, relocationFilterSymbol, "(Ljava/lang/String;Z)I"),
  NATIVE_METHOD(FakeLinker, removeAllRelocationFilterSymbol, "()V"),
};

C_API JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *unused) {
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    async_safe_fatal("JNI environment error");
  }
  original_functions = const_cast<JNINativeInterface *>(env->functions);
  android_api = GetApiLevel();
  android_api_T = __ANDROID_API_T__;
  Init();
  jclass clazz = env->FindClass("com/sanfengandroid/fakelinker/FakeLinker");
  CHECK(env->RegisterNatives(clazz, gMethods, NELEM(gMethods)) == JNI_OK);
  CHECK(fakelinker::InitJniFunctionOffset(env, clazz,
                                          env->GetStaticMethodID(clazz, gMethods[0].name, gMethods[0].signature),
                                          reinterpret_cast<void *>(FakeLinker_nativeOffset), 0x109));
  return JNI_VERSION_1_6;
}
