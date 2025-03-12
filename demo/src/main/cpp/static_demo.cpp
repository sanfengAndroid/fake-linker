//
// Created by beichen on 2025/3/12.
//
#include <string.h>

#include <alog.h>
#include <fake_linker.h>
#include <macros.h>

static int (*orig_access)(const char *pathname, int mode) = nullptr;

C_API API_PUBLIC int access(const char *pathname, int mode) {
  LOGW("static hook access function path: %s, mode: %d", pathname, mode);
  if (strncmp(pathname, "/test/hook/path", 15) == 0) {
    LOGW("fake access /test/hook/path");
    return 0;
  }
  // Does not block other calls
  return orig_access(pathname, mode);
}

C_API JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    LOGE("JNI environment error");
    return JNI_EVERSION;
  }
  if (init_fakelinker(env,
                      static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook |
                                                  FakeLinkerMode::kFMJavaRegister),
                      nullptr) != 0) {
    LOGE("fakelinker environment error");
    return JNI_ERR;
  }

  const FakeLinker *fake_linker = get_fakelinker();

  SoinfoPtr libc = fake_linker->soinfo_find(SoinfoFindType::kSTName, "libc.so", nullptr);
  if (!libc) {
    LOGE("fakelinker find libc soinfo failed");
    return JNI_ERR;
  }

  orig_access = reinterpret_cast<int (*)(const char *pathname, int mode)>(
    fake_linker->soinfo_get_export_symbol_address(libc, "access", nullptr));
  if (!orig_access) {
    LOGE("fakelinker find libc symbol access failed");
    return JNI_ERR;
  }

  SoinfoPtr fake_soinfo = fake_linker->soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr);
  if (fake_soinfo == nullptr) {
    LOGE("find self soinfo failed");
    return JNI_ERR;
  }

  // Use so itself as a global library and hook all so loaded later
  if (!fake_linker->soinfo_add_to_global(fake_soinfo)) {
    LOGE("fakelinker add global soinfo failed");
    return JNI_ERR;
  }
  const char *loaded_libs[] = {
    "libjavacore.so",
  };
  // Since libjavacore has been loaded before we load it, symbol relocation will not be triggered.
  // At this time, manually relocate it to make the hook take effect.
  if (!fake_linker->call_manual_relocation_by_names(fake_soinfo, 1, loaded_libs)) {
    LOGE("relocation libjavacore.so failed");
    return JNI_ERR;
  }
  LOGW("fakelinker static load successfully");
  return JNI_VERSION_1_6;
}