#include <android/api-level.h>
#include <dlfcn.h>

#include <cstring>

#include <alog.h>
#include <fake_linker.h>
#include <linker_version.h>
#include <macros.h>
#include <maps_util.h>
#include <proxy_jni.h>
#include <scoped_utf_chars.h>


#include "art/hook_jni_native_interface_impl.h"
#include "linker_symbol.h"


int g_log_level = FAKELINKER_LOG_LEVEL;
int g_version_code = FAKELINKER_MODULE_VERSION;
const char *g_version_name = FAKELINKER_MODULE_VERSION_NAME;
int android_api;
C_API FakeLinker g_fakelinker_export;
JNINativeInterface *original_functions = nullptr;
bool init_success = false;
JNINativeInterface *fakelinker::ProxyJNIEnv::backup_functions = nullptr;


using FakeLinkerModulePtr = void (*)(JNIEnv *, SoinfoPtr, const FakeLinker *);

static void Init(fakelinker::LinkerSymbolCategory category) {
  static bool initialized = false;
  if (!initialized) {
    soinfo::Init();
    android_namespace_t::Init();
  }
  if (!init_success) {
    init_success = fakelinker::linker_symbol.LoadSymbol(category);
  }
  initialized = true;
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
  void *hook_handle = dlopen(hook_module.c_str(), RTLD_NOW);
  if (hook_handle == nullptr) {
    LOGE("load hook module failed: %s", dlerror());
    return JNI_FALSE;
  }
  InitHookModule(env, hook_handle, hook_module.c_str());
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
  {#functionName, signature, (void *)(className##_##functionName)}

#ifndef NELEM
#define NELEM(x) ((int)(sizeof(x) / sizeof((x)[0])))
#endif

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(FakeLinker, entrance, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(FakeLinker, setLogLevel, "(I)V"),
  NATIVE_METHOD(FakeLinker, relocationFilterSymbol, "(Ljava/lang/String;Z)I"),
  NATIVE_METHOD(FakeLinker, removeAllRelocationFilterSymbol, "()V"),
};

C_API int init_fakelinker(JNIEnv *env, FakeLinkerMode mode, const char *java_class_name) {
  android_api = android_get_device_api_level();
  static bool native_hook_initialized = false;
  static bool java_register_initialized = false;
  if (mode & FakeLinkerMode::kFMSoinfo) {
    int symbol_type = fakelinker::LinkerSymbolCategory::kLinkerAll;
    if (mode &
        (FakeLinkerMode::kInitLinkerDebug | FakeLinkerMode::kInitLinkerDlopenDlSym |
         FakeLinkerMode::kInitLinkerNamespace | FakeLinkerMode::kInitLinkerHandler |
         FakeLinkerMode::kInitLinkerMemory)) {
      symbol_type = 0;
      if (mode & FakeLinkerMode::kInitLinkerDebug) {
        symbol_type |= fakelinker::LinkerSymbolCategory::kLinkerDebug;
      }
      if (mode & FakeLinkerMode::kInitLinkerDlopenDlSym) {
        symbol_type |= fakelinker::LinkerSymbolCategory::kDlopenDlSym;
      }
      if (mode & FakeLinkerMode::kInitLinkerNamespace) {
        symbol_type |= fakelinker::LinkerSymbolCategory::kNamespace;
      }
      if (mode & FakeLinkerMode::kInitLinkerHandler) {
        symbol_type |= fakelinker::LinkerSymbolCategory::kSoinfoHandler;
      }
      if (mode & FakeLinkerMode::kInitLinkerMemory) {
        symbol_type |= fakelinker::LinkerSymbolCategory::kSoinfoMemory;
      }
    }

    Init(static_cast<fakelinker::LinkerSymbolCategory>(symbol_type));
    if (!init_success) {
      return 1;
    }
  }

  if ((mode & FakeLinkerMode::kFMNativeHook) && !native_hook_initialized) {
    if (env == nullptr) {
      LOGE("JNIEnv is a null pointer and cannot register native hook");
      return 2;
    }
    original_functions = const_cast<JNINativeInterface *>(env->functions);
    native_hook_initialized = fakelinker::DefaultInitJniFunctionOffset(env);
    if (!native_hook_initialized) {
      return 3;
    }
  }

  if ((mode & (FakeLinkerMode::kFMJavaRegister | FakeLinkerMode::kFMForceJavaRegister)) && !java_register_initialized) {
    if (env == nullptr) {
      LOGE("JNIEnv is a null pointer and cannot register fakelinker java api");
      return 4;
    }
    jclass clazz = env->FindClass(java_class_name ? java_class_name : "com/sanfengandroid/fakelinker/FakeLinker");
    if (clazz == nullptr) {
      env->ExceptionClear();
      return (mode & FakeLinkerMode::kFMJavaRegister) && java_class_name == nullptr ? 0 : 5;
    }
    java_register_initialized = env->RegisterNatives(clazz, gMethods, NELEM(gMethods)) == JNI_OK;
    if (!java_register_initialized) {
      return (mode & FakeLinkerMode::kFMJavaRegister) && java_class_name == nullptr ? 0 : 6;
    }
  }
  return 0;
}
