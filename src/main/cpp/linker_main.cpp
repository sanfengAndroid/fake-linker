#include "linker_main.h"

#include <dlfcn.h>

#include <alog.h>
#include <maps_util.h>
#include <module_config.h>
#include <scoped_utf_chars.h>
#include <cstring>

#include "linker/fake_linker.h"
#include "linker/linker_util.h"
#include "linker/linker_globals.h"
#include "linker/art/hook_jni_native_interface_impl.h"

int g_log_level = HOOK_LOG_LEVEL;
int g_version_code = MODULE_VERSION;
const char *g_version_name = MODULE_VERSION_NAME;

RemoteInvokeInterface gRemoteInvokeInterface = {
        HookJniNativeInterface,
        HookJniNativeInterfaces,
        call_soinfo_function,
        call_common_function,
#if __ANDROID_API__ >= __ANDROID_API_N__
        call_namespace_function,
        reinterpret_cast<void *(*)(const char *, const char *, const char *, uint64_t, const char *, void *, const void *)>(ProxyLinker::CallCreateNamespace),
        reinterpret_cast<void *(*)(const char *, int, const void *, void *)>(ProxyLinker::CallDoDlopen),
        ProxyLinker::CallDoDlsym,
#else
        reinterpret_cast<void *(*)(const char *, int, const void *)>(ProxyLinker::CallDoDlopen),
        ProxyLinker::CallDoDlsym,
#endif
        RegisterNativeAgain
};

JNINativeInterface *original_functions;

static void
(*fake_load_library_init_ptr)(JNIEnv *env, void *fake_soinfo, const RemoteInvokeInterface *interface, const char *cache_path, const char *config_path, const char *process_name);

static void InitHookModule(JNIEnv *env, void *module, const char *cache_path, const char *config_path, const char *process_name) {
    fake_load_library_init_ptr = reinterpret_cast<void (*)(JNIEnv *, void *, const RemoteInvokeInterface *, const char *, const char *, const char *)>(dlsym(module,
                                                                                                                                                             HOOK_LIB_INIT_NAME));
    soinfo *this_so = ProxyLinker::FindContainingLibrary(reinterpret_cast<const void *>(&ProxyLinker::FindContainingLibrary));
    fake_load_library_init_ptr(env, this_so, &gRemoteInvokeInterface, cache_path, config_path, process_name);
}

static void FakeLinker_nativeOffset(JNIEnv *env, jclass clazz) {
    LOGE("This test native offset");
}

static void InitArt(JNIEnv *env, jclass clazz) {
    JNINativeMethod methods[] = {{"nativeOffset", "()V", reinterpret_cast<void *>(FakeLinker_nativeOffset)}};
    CHECK(env->RegisterNatives(clazz, methods, 1) == JNI_OK);
    CHECK(InitJniFunctionOffset(env, clazz, env->GetStaticMethodID(clazz, methods[0].name, methods[0].signature), reinterpret_cast<void *>(FakeLinker_nativeOffset), 0x109));
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_sanfengandroid_fakelinker_FakeLinker_entrance(JNIEnv *env, jclass clazz, jstring hook_module_path, jstring cache_dir, jstring config_path, jstring process_name) {
    ScopedUtfChars cache(env, cache_dir);
    ScopedUtfChars hook_module(env, hook_module_path);
    ScopedUtfChars process_(env, process_name);
    ScopedUtfChars config(env, config_path);

    ProxyLinker::Init();
    InitArt(env, clazz);
    original_functions = const_cast<JNINativeInterface *>(env->functions);
    void *hookHandle = dlopen(hook_module.c_str(), RTLD_NOW);
    if (hookHandle == nullptr) {
        LOGE("load hook module failed: %s", dlerror());
        return JNI_FALSE;
    }
    InitHookModule(env, hookHandle, cache.c_str(), config.c_str(), process_.c_str());
    return JNI_TRUE;
}

extern "C" JNIEXPORT jint JNICALL Java_com_sanfengandroid_fakelinker_FakeLinker_relinkSpecialFilterSymbol(JNIEnv *env, jclass clazz, jstring symbol_name, jboolean add) {
    ScopedUtfChars symbol(env, symbol_name);
    int error_code;

    gRemoteInvokeInterface.CallCommonFunction(add ? kCFAddRelinkFilterSymbol : kCFRemoveRelinkFilterSymbol, kSPSymbol, symbol.c_str(), kSPNull, nullptr, &error_code);
    if (error_code != kErrorNo) {
        LOGE("Add or remove relink symbol '%s' failed, error code: %d", symbol.c_str(), error_code);
    }
    return error_code;
}
extern "C"
JNIEXPORT void JNICALL Java_com_sanfengandroid_fakelinker_FakeLinker_setLogLevel(JNIEnv *env, jclass clazz, jint level) {
    g_log_level = level;
}