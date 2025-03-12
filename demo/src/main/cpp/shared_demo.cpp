//
// Created by beichen on 2025/3/12.
//
#include <string.h>

#include <alog.h>
#include <fake_linker.h>
#include <macros.h>

static int (*orig_access)(const char *pathname, int mode) = nullptr;

int g_log_level = 0;

C_API API_PUBLIC int access(const char *pathname, int mode) {
  LOGW("shared hook access function path: %s, mode: %d", pathname, mode);
  if (strncmp(pathname, "/test/hook/path", 15) == 0) {
    LOGW("fake access /test/hook/path");
    return 0;
  }
  // Does not block other calls
  return orig_access(pathname, mode);
}

C_API JNIEXPORT void fakelinker_module_init(JNIEnv *env, SoinfoPtr fake_soinfo, const FakeLinker *fake_linker) {
  if (!fake_linker->is_init_success()) {
    LOGE("fakelinker environment initialization failed, cannot continue testing");
    return;
  }
  SoinfoPtr libc = fake_linker->soinfo_find(SoinfoFindType::kSTName, "libc.so", nullptr);
  if (!libc) {
    LOGE("fakelinker find libc soinfo failed");
    return;
  }

  orig_access = reinterpret_cast<int (*)(const char *pathname, int mode)>(
    fake_linker->soinfo_get_export_symbol_address(libc, "access", nullptr));
  if (!orig_access) {
    LOGE("fakelinker find libc symbol access failed");
    return;
  }
  // Use so itself as a global library and hook all so loaded later
  if (!fake_linker->soinfo_add_to_global(fake_soinfo)) {
    LOGE("fakelinker add global soinfo failed");
    return;
  }
  const char *loaded_libs[] = {
    "libjavacore.so",
  };
  // Since libjavacore has been loaded before we load it, symbol relocation will not be triggered.
  // At this time, manually relocate it to make the hook take effect.
  if (!fake_linker->call_manual_relocation_by_names(fake_soinfo, 1, loaded_libs)) {
    LOGE("relocation libjavacore.so failed");
    return;
  }
  LOGW("fakelinker shared load successfully");
}