#include <android/api-level.h>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <jni.h>

#include <fakelinker/elf_reader.h>
#include <fakelinker/fake_linker.h>
#include "../linker/linker_globals.h"
#include "../linker/linker_symbol.h"

using namespace fakelinker;

extern FakeLinker g_fakelinker_export;

static bool endswith(const char *name, const std::string &suffix) {
  if (!name) {
    return false;
  }
  if (suffix == name) {
    return true;
  }
  int len = strlen(name);
  if (len > suffix.length()) {
    return strncmp(name + len - suffix.length(), suffix.c_str(), suffix.length()) == 0;
  }
  return false;
}

constexpr const char *get_arch_name() {
#if defined(__arm__)
  return "arm";
#elif defined(__aarch64__)
  return "aarch64";
#elif defined(__i386__)
  return "x86";
#elif defined(__x86_64__)
  return "x86_64";
#else
#error "not support arch";
#endif
}

static constexpr bool isStaticTest() {
#ifdef FAKELINKER_STATIC_TEST
  return true;
#else
  return false;
#endif
}

static int GetApiLevel() {
  int api_level = android_get_device_api_level();
  return (api_level > 0) ? api_level : -1;
}

static void Init() { init_fakelinker(nullptr, FakeLinkerMode::kFMSoinfo, nullptr); }

static void init_env() {
  android_api = GetApiLevel();
  LOGI("current android api: %d", android_api);
  Init();
}

C_API JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
    async_safe_fatal("JNI environment error");
  }
  init_fakelinker(env,
                  static_cast<FakeLinkerMode>(FakeLinkerMode::kFMSoinfo | FakeLinkerMode::kFMNativeHook |
                                              FakeLinkerMode::kFMJavaRegister),
                  nullptr);
  return JNI_VERSION_1_6;
}

TEST(Fakelinker, initTest) {
  if (!isStaticTest()) {
    init_env();
    ASSERT_EQ(init_fakelinker(nullptr, FakeLinkerMode::kFMNativeHook, nullptr), 0) << "init native hook failed";
  }
  ASSERT_TRUE(g_fakelinker_export.is_init_success()) << "Fakelinker init";
  ASSERT_TRUE(g_fakelinker_export.set_ld_debug_verbosity(3)) << "open linker log";
}

TEST(FakeLinker, soinfoTest) {
  int error;
  SoinfoPtr *soinfo_array;
  EXPECT_TRUE(g_fakelinker_export.soinfo_query_all(&soinfo_array, &error) > 0) << "soinfo_query_all";
  EXPECT_TRUE(error == FakeLinkerError::kErrorNo);
  g_fakelinker_export.free_inside_memory(soinfo_array);

  SoinfoPtr libc_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, "libc.so", &error);
  EXPECT_TRUE(libc_soinfo != nullptr) << "soinfo_find for name";

  SoinfoPtr thiz_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr);
  EXPECT_TRUE(thiz_soinfo != nullptr) << "soinfo_find for address";

  if (android_api >= __ANDROID_API_N__) {
    SoinfoHandle handle = g_fakelinker_export.soinfo_get_handle(thiz_soinfo, nullptr);
    EXPECT_TRUE(handle != nullptr) << "android 7.0+ soinfo_get_handle";
    if (handle) {
      EXPECT_TRUE(thiz_soinfo == g_fakelinker_export.soinfo_find(SoinfoFindType::kSTHandle, handle, nullptr))
        << "android 7.0+ soinfo_find for handle";
    }
  } else {
    SoinfoHandle handle = g_fakelinker_export.soinfo_get_handle(thiz_soinfo, &error);
    EXPECT_EQ(handle, nullptr) << "android 7.0- soinfo_get_handle unavailable";
    EXPECT_EQ(error, FakeLinkerError::kErrorApiLevel) << "android 7.0- soinfo_get_handle unavailable";
  }

  SoinfoAttributes attr{};
  error = g_fakelinker_export.soinfo_get_attribute(libc_soinfo, &attr);
  EXPECT_TRUE(error == FakeLinkerError::kErrorNo) << "soinfo_get_attribute";
  if (error == FakeLinkerError::kErrorNo) {
    EXPECT_TRUE(attr.soinfo_ptr == libc_soinfo) << "soinfo_get_attribute field soinfo_ptr";
    EXPECT_TRUE(endswith(attr.so_name, "libc.so")) << "soinfo_get_attribute field so_name";
    EXPECT_TRUE(attr.real_path != nullptr && strlen(attr.real_path) > 0) << "soinfo_get_attribute field real_path";
    EXPECT_TRUE(attr.base != 0 && PAGE_START(attr.base) == attr.base) << "soinfo_get_attribute field base";
    EXPECT_TRUE(attr.size > 0) << "soinfo_get_attribute field size";
  }

  const char *name = g_fakelinker_export.soinfo_get_name(libc_soinfo, &error);
  EXPECT_TRUE(error == FakeLinkerError::kErrorNo && endswith(name, "libc.so")) << "soinfo_get_name";

  const char *real_path = g_fakelinker_export.soinfo_get_realpath(libc_soinfo, &error);
  EXPECT_TRUE(error == FakeLinkerError::kErrorNo && endswith(real_path, "libc.so")) << "soinfo_get_realpath";
  EXPECT_TRUE(g_fakelinker_export.soinfo_get_linker_soinfo() != nullptr) << "soinfo_get_linker_soinfo";

  SymbolAddress *dlclose_import = g_fakelinker_export.soinfo_get_import_symbol_address(libc_soinfo, "dlclose", nullptr);

  EXPECT_TRUE(dlclose_import != nullptr && *dlclose_import == dlclose) << "soinfo_get_import_symbol_address";
  SymbolAddress strlen_export = g_fakelinker_export.soinfo_get_export_symbol_address(libc_soinfo, "strlen", nullptr);
  EXPECT_TRUE(strlen_export == strlen) << "soinfo_get_export_symbol_address";
  EXPECT_TRUE(g_fakelinker_export.soinfo_get_fakelinker_soinfo() != nullptr) << "soinfo_get_fakelinker_soinfo";

  EXPECT_TRUE(g_fakelinker_export.soinfo_add_to_global(thiz_soinfo)) << "soinfo_add_to_global";
  EXPECT_TRUE(g_fakelinker_export.soinfo_is_global(thiz_soinfo, nullptr)) << "soinfo_is_global";

  EXPECT_TRUE(g_fakelinker_export.soinfo_remove_global(thiz_soinfo)) << "soinfo_remove_global";
  EXPECT_FALSE(g_fakelinker_export.soinfo_is_global(thiz_soinfo, nullptr)) << "soinfo_is_global";
}

TEST(FakeLinker, namespaceTest) {
  SoinfoPtr thiz = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr);
  ASSERT_TRUE(thiz);
  int error;
  if (android_api < __ANDROID_API_N__) {
    AndroidNamespacePtr thiz_np =
      g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, thiz, &error);
    EXPECT_TRUE(thiz_np == nullptr && error == kErrorApiLevel) << "android 7.0- namespace unavailable";
    return;
  }
  AndroidNamespacePtr thiz_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, thiz, &error);
  EXPECT_TRUE(thiz_np != nullptr && error == kErrorNo) << "android_namespace_find";
  if (thiz_np != nullptr && error == kErrorNo) {
    const char *name = g_fakelinker_export.android_namespace_get_name(thiz_np, &error);
    EXPECT_TRUE(name != nullptr && error == FakeLinkerError::kErrorNo) << "android_namespace_get_name";
  }

  AndroidNamespacePtr *array = nullptr;
  int len = g_fakelinker_export.android_namespace_query_all(&array, &error);
  EXPECT_TRUE(array != nullptr && error == FakeLinkerError::kErrorNo && len > 0) << "android_namespace_query_all";
  g_fakelinker_export.free_inside_memory(array);

  SoinfoPtr *solist;
  len = g_fakelinker_export.android_namespace_get_all_soinfo(thiz_np, &solist, &error);

  EXPECT_TRUE(len > 0 && error == FakeLinkerError::kErrorNo) << "android_namespace_get_all_soinfo";
  g_fakelinker_export.free_inside_memory(solist);

  if (android_api >= __ANDROID_API_O__) {
    AndroidLinkNamespacePtr links;
    len = g_fakelinker_export.android_namespace_get_link_namespace(thiz_np, &links, &error);
    EXPECT_EQ(error, FakeLinkerError::kErrorNo) << "android_namespace_get_link_namespace";
  }

  error = g_fakelinker_export.android_namespace_add_global_soinfo(thiz_np, thiz);
  EXPECT_EQ(error, FakeLinkerError::kErrorNo) << "android_namespace_add_global_soinfo";

  len = g_fakelinker_export.android_namespace_get_global_soinfos(thiz_np, &solist, &error);
  EXPECT_TRUE(len >= 1 && error == FakeLinkerError::kErrorNo) << "android_namespace_get_global_soinfos";

  bool found_global = false;
  for (int i = 0; i < len; ++i) {
    if (solist[i] == thiz) {
      found_global = true;
      break;
    }
  }

  EXPECT_TRUE(found_global) << "android_namespace_add_global_soinfo verify";

  EXPECT_EQ(g_fakelinker_export.android_namespace_remove_soinfo(thiz_np, thiz, true), FakeLinkerError::kErrorNo)
    << "android_namespace_remove_soinfo";
  EXPECT_FALSE(g_fakelinker_export.soinfo_is_global(thiz, nullptr)) << "soinfo_is_global";
  EXPECT_EQ(g_fakelinker_export.android_namespace_add_soinfo(thiz_np, thiz), FakeLinkerError::kErrorNo)
    << "android_namespace_add_soinfo";
  EXPECT_EQ(g_fakelinker_export.android_namespace_remove_soinfo(thiz_np, thiz, false), FakeLinkerError::kErrorNo)
    << "android_namespace_remove_soinfo 2";

  if (android_api >= __ANDROID_API_Q__) {
    const char *libs;
    len = g_fakelinker_export.android_namespace_get_white_list(thiz_np, &libs, &error);
    EXPECT_EQ(error, FakeLinkerError::kErrorNo) << "android_namespace_get_white_list";
    g_fakelinker_export.free_inside_memory(libs);

    error = g_fakelinker_export.android_namespace_add_soinfo_whitelist(thiz_np, "libtest.so");
    EXPECT_EQ(error, FakeLinkerError::kErrorNo) << "android_namespace_add_soinfo_whitelist";
    if (error == FakeLinkerError::kErrorNo) {
      EXPECT_EQ(g_fakelinker_export.android_namespace_get_white_list(thiz_np, &libs, &error), len + 1)
        << "android_namespace_add_soinfo_whitelist verify";
      EXPECT_EQ(g_fakelinker_export.android_namespace_remove_whitelist(thiz_np, "libtest.so"),
                FakeLinkerError::kErrorNo)
        << "android_namespace_remove_whitelist";
      g_fakelinker_export.free_inside_memory(libs);
    }
  } else {
    const char *libs;
    len = g_fakelinker_export.android_namespace_get_white_list(thiz_np, &libs, &error);
    EXPECT_TRUE(len == 0 && error == FakeLinkerError::kErrorApiLevel)
      << "android 10- android_namespace_get_white_list unavailable";
  }

  EXPECT_EQ(g_fakelinker_export.android_namespace_add_ld_library_path(thiz_np, "/data/local/add_ld_library"),
            FakeLinkerError::kErrorNo)
    << "android_namespace_add_ld_library_path";
  // 目前还不能修改 default_library 和 permitted_library
  EXPECT_EQ(g_fakelinker_export.android_namespace_add_default_library_path(thiz_np, "data/local/add_default_library"),
            FakeLinkerError::kErrorUnavailable)
    << "android_namespace_add_default_library_path";
  EXPECT_EQ(
    g_fakelinker_export.android_namespace_add_permitted_library_path(thiz_np, "/data/local/add_permitted_library"),
    FakeLinkerError::kErrorUnavailable)
    << "android_namespace_add_permitted_library_path";

  EXPECT_EQ(g_fakelinker_export.soinfo_add_second_namespace(thiz, thiz_np), FakeLinkerError::kErrorNo)
    << "soinfo_add_second_namespace";
  EXPECT_EQ(g_fakelinker_export.soinfo_remove_second_namespace(thiz, thiz_np), FakeLinkerError::kErrorNo)
    << "soinfo_remove_second_namespace";
  auto libc_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, "libc.so", nullptr);
  auto libc_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, libc_soinfo, nullptr);
  EXPECT_NE(libc_np, nullptr) << "android_namespace_find";

  const char *default_path = is64BitBuild() ? "/system/lib64" : "/system/lib";
  std::string new_namespace_name = "linker_test";
  if (android_api <= __ANDROID_API_N_MR1__) {
    uint64_t addr = g_fakelinker_export.find_library_symbol(
      is64BitBuild() ? "linker64" : "linker", "__dl__ZL30g_public_namespace_initialized", FindSymbolType::kInternal);
    EXPECT_NE(addr, 0) << "find internal symbol";
    if (addr != 0) {
      *reinterpret_cast<bool *>(addr) = true;
    }
  }
  AndroidNamespacePtr new_create_np = g_fakelinker_export.android_namespace_create(
    new_namespace_name.c_str(), nullptr, default_path, 1, nullptr, libc_np, nullptr);
  android_namespace_t_R *ars = static_cast<android_namespace_t_R *>(new_create_np);
  EXPECT_TRUE(new_create_np) << "android_namespace_create";
  EXPECT_NE(thiz_np, new_create_np);
  EXPECT_EQ(g_fakelinker_export.soinfo_change_namespace(thiz, new_create_np), FakeLinkerError::kErrorNo)
    << "soinfo_change_namespace";

  len = g_fakelinker_export.android_namespace_query_all(&array, nullptr);
  EXPECT_TRUE(len > 1);

  bool check_namespace = false;

  for (int i = 0; i < len; ++i) {
    const char *name = g_fakelinker_export.android_namespace_get_name(array[i], nullptr);
    LOGI("found namespace name: %s", name);
    if (name && new_namespace_name == name) {
      check_namespace = true;
    }
  }
  EXPECT_TRUE(check_namespace) << "check namespace create";

  AndroidLinkNamespacePtr new_thiz_np =
    g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, thiz, nullptr);
  EXPECT_NE(new_thiz_np, thiz_np);
  EXPECT_EQ(new_thiz_np, new_create_np);
  EXPECT_EQ(g_fakelinker_export.soinfo_change_namespace(thiz, thiz_np), FakeLinkerError::kErrorNo)
    << "soinfo_change_namespace";
  new_thiz_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, thiz, nullptr);
  EXPECT_EQ(new_thiz_np, thiz_np);
}

#define CHECK_MEMBER(name)                                                                                             \
  template <typename T>                                                                                                \
  struct has_member_##name {                                                                                           \
    template <typename _T>                                                                                             \
    static auto check(_T) -> typename std::decay_t<decltype(_T::name)>;                                                \
    static void check(...);                                                                                            \
    using type = decltype(check(std::declval<T>()));                                                                   \
    enum { value = !std::is_void<type>::value };                                                                       \
  };

CHECK_MEMBER(soname_);
CHECK_MEMBER(ld_library_paths_);
CHECK_MEMBER(default_library_paths_);
CHECK_MEMBER(permitted_paths_);
CHECK_MEMBER(whitelisted_libs_);

template <typename T>
void print_soinfo_offset() {
  const char *name = typeid(T).name();
  LOGI("type: %s, arch: %s", name, get_arch_name());
  if constexpr (has_member_soname_<T>::value) {
    LOGI("\tsoname offset: %zu", offsetof(T, soname_));
  }
  LOGI("\tchildren_ offset: %zu", offsetof(T, children_));
  LOGI("\tparents_ offset: %zu", offsetof(T, parents_));
  LOGI("\tst_dev_ offset: %zu", offsetof(T, st_dev_));
  LOGI("\tst_dev_ offset: %zu", offsetof(T, st_dev_));
  LOGI("\tst_ino_ offset: %zu", offsetof(T, st_ino_));

  if constexpr (has_member_ld_library_paths_<T>::value) {
    LOGI("\tandroid_namespace ld_library_paths_ offset: %zu", offsetof(T, ld_library_paths_));
  }
  if constexpr (has_member_default_library_paths_<T>::value) {
    LOGI("\tandroid_namespace default_library_paths_ offset: %zu", offsetof(T, default_library_paths_));
  }
  if constexpr (has_member_permitted_paths_<T>::value) {
    LOGI("\tandroid_namespace permitted_paths_ offset: %zu", offsetof(T, permitted_paths_));
  }

  if constexpr (has_member_whitelisted_libs_<T>::value) {
    LOGI("\tandroid_namespace whitelisted_libs_ offset: %zu", offsetof(T, whitelisted_libs_));
  }
}

TEST(FakeLinker, soinfoOffsetTest) {
  ElfReader reader;
  ASSERT_TRUE(reader.LoadFromMemory("libc.so")) << "test load library from memory";

  soinfo *libc_soinfo =
    reinterpret_cast<soinfo *>(g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, "libc.so", nullptr));
  ASSERT_TRUE(libc_soinfo);
  EXPECT_EQ(libc_soinfo->load_bias(), reader.load_bias()) << "verify load_bias";
  EXPECT_EQ(libc_soinfo->strtab(), reader.strtab_) << "verify strtab_";
  if (android_api >= __ANDROID_API_L_MR1__) {
    EXPECT_EQ(libc_soinfo->strtab_size(), reader.strtab_size_) << "verify strtab_size";
  }
  EXPECT_EQ(libc_soinfo->symtab(), reader.symtab_) << "verify symtab_";
  if (reader.gnu_nbucket_ != 0 && android_api >= __ANDROID_API_M__) {
    EXPECT_EQ(libc_soinfo->gnu_nbucket(), reader.gnu_nbucket_) << "verify gnu_nbucket_";
    EXPECT_EQ(libc_soinfo->gnu_bucket(), reader.gnu_bucket_) << "verify gnu_bucket_";
    EXPECT_EQ(libc_soinfo->gnu_chain(), reader.gnu_chain_) << "verify gnu_chain_";
    EXPECT_EQ(libc_soinfo->gnu_maskwords(), reader.gnu_maskwords_) << "verify gnu_maskwords_";
    EXPECT_EQ(libc_soinfo->gnu_shift2(), reader.gnu_shift2_) << "verify gnu_shift2_";
    EXPECT_EQ(libc_soinfo->gnu_bloom_filter(), reader.gnu_bloom_filter_) << "verify gnu_bloom_filter_";
  } else {
    EXPECT_EQ(libc_soinfo->nbucket(), reader.nbucket_) << "verify nbucket_";
    EXPECT_EQ(libc_soinfo->nchain(), reader.nchain_) << "verify nchain_";
    EXPECT_EQ(libc_soinfo->bucket(), reader.bucket_) << "verify bucket_";
    EXPECT_EQ(libc_soinfo->chain(), reader.chain_) << "verify chain_";
  }
}

TEST(FakeLinker, loadTest) {
  auto dlopen_ptr = g_fakelinker_export.get_dlopen_inside_func_ptr();
  EXPECT_TRUE(dlopen_ptr) << "get_dlopen_inside_func_ptr";
  auto dlsym_ptr = g_fakelinker_export.get_dlsym_inside_func_ptr();
  EXPECT_TRUE(dlsym_ptr) << "get_dlsym_inside_func_ptr";

  void *handle = dlopen_ptr("libc.so", RTLD_NOW, nullptr, {});

  ASSERT_TRUE(handle) << "inside dlopen call";
  EXPECT_EQ(dlsym_ptr(handle, "strncmp", nullptr), &strncmp) << "inside dlsym call";

  const char *name = "dlopen";
  if (android_api >= __ANDROID_API_O__) {
    name = "__loader_dlopen";
  }
  EXPECT_TRUE(g_fakelinker_export.get_linker_export_symbol(name, nullptr)) << "get_linker_export_symbol";
}

C_API API_PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz) { return 0; }

C_API API_PUBLIC void android_set_abort_message(const char *msg) {
  LOGI("only test relocate android_set_abort_message");
}

static void *dlope_library(const char *name, void *caller_address) {
  if (caller_address) {
    return g_fakelinker_export.call_dlopen_inside(name, RTLD_NOW, caller_address, nullptr);
  }
  return dlopen(name, RTLD_NOW);
}

TEST(FakeLinker, relocateTest) {
  auto log_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, "liblog.so", nullptr);
  ASSERT_TRUE(log_soinfo) << "soinfo_find";

  auto thiz = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTAddress, nullptr, nullptr);
  ASSERT_TRUE(thiz) << "soinfo find";

  SoinfoAttributes attr{};
  EXPECT_EQ(g_fakelinker_export.soinfo_get_attribute(thiz, &attr), FakeLinkerError::kErrorNo);

  // test liblog import
  ASSERT_TRUE(g_fakelinker_export.soinfo_add_to_global(thiz));

  EXPECT_TRUE(g_fakelinker_export.call_manual_relocation_by_soinfo(thiz, log_soinfo))
    << "call_manual_relocation_by_soinfo";
  SymbolAddress *message_addr =
    g_fakelinker_export.soinfo_get_import_symbol_address(log_soinfo, "android_set_abort_message", nullptr);
  ASSERT_TRUE(message_addr) << "find import android_set_abort_message";
  EXPECT_EQ(*message_addr, &android_set_abort_message) << "call_manual_relocation_by_soinfo verify";

  const char *test_library = nullptr;
  const char *test_function = "gettimeofday";

  std::vector<const char *> test_librarys = {"libssl.so",      "libpcap.so", "libext2_uuid.so",
                                             "libcups.so",     "libcurl.so", "libpac.so",
                                             "libloc_stub.so", "libavlm.so", "libbcinfo.so"};

  void *handle = nullptr;
  void *caller_address = nullptr;
  AndroidNamespacePtr log_np = nullptr;
  if (android_api >= __ANDROID_API_N__) {
    log_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, log_soinfo, nullptr);
    ASSERT_TRUE(log_np) << "find liblog.so namespace failed.";
    caller_address = g_fakelinker_export.android_namespace_get_caller_address(log_np, nullptr);
    ASSERT_TRUE(caller_address) << "get liblog.so namespace caller address failed";
  }
  SoinfoPtr test_soinfo = nullptr;
  std::string library_dir = is64BitBuild() ? "/system/lib64/" : "/system/lib/";
  for (auto name : test_librarys) {
    std::string path = library_dir + name;
    if (access(path.c_str(), F_OK) != 0) {
      continue;
    }
    if (android_api >= __ANDROID_API_N__) {
      test_soinfo = g_fakelinker_export.soinfo_find_in_namespace(SoinfoFindType::kSTName, name, log_np, nullptr);
    } else {
      test_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, name, nullptr);
    }
    if (!test_soinfo) {
      test_library = name;
      break;
    }
  }
  ASSERT_TRUE(test_library) << "The current device cannot test library relocation, and the test library has already "
                               "been loaded";
  LOGW("current test so library: %s", test_library);
  handle = dlope_library(test_library, caller_address);
  ASSERT_TRUE(handle) << "dlopen " << test_library << " failed";

  test_soinfo = nullptr;
  if (android_api >= __ANDROID_API_N__) {
    test_soinfo = g_fakelinker_export.soinfo_find_in_namespace(SoinfoFindType::kSTName, test_library, log_np, nullptr);
  } else {
    test_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, test_library, nullptr);
  }
  ASSERT_TRUE(test_soinfo) << "find " << test_library << " soinfo failed";

  SymbolAddress *test_import_function =
    g_fakelinker_export.soinfo_get_import_symbol_address(test_soinfo, test_function, nullptr);
  ASSERT_TRUE(test_import_function) << "find " << test_library << " import symbol: " << test_function;
  EXPECT_EQ(*test_import_function, reinterpret_cast<void *>(&gettimeofday))
    << "call_manual_relocation_by_soinfo verify";
  ASSERT_EQ(dlclose(handle), 0) << "dlclose";

  // remove global
  g_fakelinker_export.soinfo_remove_global(thiz);

  EXPECT_FALSE(g_fakelinker_export.soinfo_is_global(thiz, nullptr)) << "remove global";
  handle = dlope_library(test_library, caller_address);
  if (android_api >= __ANDROID_API_N__) {
    test_soinfo = g_fakelinker_export.soinfo_find_in_namespace(SoinfoFindType::kSTName, test_library, log_np, nullptr);
  } else {
    test_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, test_library, nullptr);
  }
  ASSERT_TRUE(test_soinfo) << "find " << test_library << " soinfo failed";
  test_import_function = g_fakelinker_export.soinfo_get_import_symbol_address(test_soinfo, test_function, nullptr);
  ASSERT_TRUE(test_import_function) << "find import";
  if (android_api < __ANDROID_API_M__) {
    EXPECT_NE(*test_import_function, &gettimeofday) << "call_manual_relocation_by_soinfo verify";
  } else {
    EXPECT_NE(*test_import_function, &gettimeofday) << "call_manual_relocation_by_soinfo verify";
  }
  ASSERT_EQ(dlclose(handle), 0) << "dlclose";

  if (android_api >= __ANDROID_API_N__) {
    const char *default_path = is64BitBuild() ? "/system/lib64" : "/system/lib";
    auto libc_soinfo = g_fakelinker_export.soinfo_find(SoinfoFindType::kSTName, "libc.so", nullptr);
    auto libc_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, libc_soinfo, nullptr);
    EXPECT_NE(libc_np, nullptr) << "android_namespace_find";

    if (android_api <= __ANDROID_API_N_MR1__) {
      uint64_t addr = g_fakelinker_export.find_library_symbol(
        is64BitBuild() ? "linker64" : "linker", "__dl__ZL30g_public_namespace_initialized", FindSymbolType::kInternal);
      EXPECT_NE(addr, 0) << "find internal symbol";
      if (addr != 0) {
        *reinterpret_cast<bool *>(addr) = true;
      }
    }
    AndroidNamespacePtr new_create_np =
      g_fakelinker_export.android_namespace_create("linker_test2", nullptr, default_path, 1, nullptr, libc_np, nullptr);

    ASSERT_TRUE(new_create_np) << "android_namespace_create";

    EXPECT_EQ(g_fakelinker_export.soinfo_change_namespace(thiz, new_create_np), FakeLinkerError::kErrorNo);
    EXPECT_EQ(g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, thiz, nullptr), new_create_np)
      << "verify change namespace";

    EXPECT_TRUE(g_fakelinker_export.soinfo_add_to_global(thiz));
    void *caller = g_fakelinker_export.android_namespace_get_caller_address(libc_np, nullptr);
    EXPECT_TRUE(caller) << "android_namespace_get_caller_address ";

    handle = g_fakelinker_export.call_dlopen_inside(test_library, RTLD_NOW, caller, nullptr);
    ASSERT_TRUE(handle) << "call_dlopen_inside";

    test_soinfo = g_fakelinker_export.soinfo_find_in_namespace(SoinfoFindType::kSTName, test_library, libc_np, nullptr);

    ASSERT_TRUE(test_soinfo) << "cross namespace find soinfo";
    AndroidNamespacePtr test_np =
      g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, test_soinfo, nullptr);
    EXPECT_TRUE(test_np);
    EXPECT_EQ(test_np, libc_np) << "dlopen namespace verify";

    test_import_function = g_fakelinker_export.soinfo_get_import_symbol_address(test_soinfo, test_function, nullptr);
    ASSERT_TRUE(test_import_function) << "find import";
    EXPECT_EQ(*test_import_function, &gettimeofday) << "call_manual_relocation_by_soinfo namespace verify";
    ASSERT_EQ(dlclose(handle), 0);

    ASSERT_TRUE(g_fakelinker_export.soinfo_remove_global(thiz));
    handle = g_fakelinker_export.call_dlopen_inside(test_library, RTLD_NOW, caller, nullptr);
    ASSERT_TRUE(handle) << "call_dlopen_inside";
    test_soinfo = g_fakelinker_export.soinfo_find_in_namespace(SoinfoFindType::kSTName, test_library, libc_np, nullptr);
    ASSERT_TRUE(test_soinfo) << "cross namespace find soinfo";
    test_np = g_fakelinker_export.android_namespace_find(NamespaceFindType::kNPSoinfo, test_soinfo, nullptr);
    EXPECT_TRUE(test_np);
    EXPECT_EQ(test_np, libc_np) << "dlopen namespace verify";

    test_import_function = g_fakelinker_export.soinfo_get_import_symbol_address(test_soinfo, test_function, nullptr);
    ASSERT_TRUE(test_import_function) << "find import";
    EXPECT_NE(*test_import_function, &gettimeofday) << "call_manual_relocation_by_soinfo namespace verify";
    ASSERT_EQ(dlclose(handle), 0);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  print_soinfo_offset<soinfoL>();
  print_soinfo_offset<soinfoL1>();
  print_soinfo_offset<soinfoM>();
  print_soinfo_offset<soinfoN>();
  print_soinfo_offset<soinfoN1>();
  print_soinfo_offset<soinfoO>();
  print_soinfo_offset<soinfoP>();
  print_soinfo_offset<soinfoQ>();
  print_soinfo_offset<soinfoR>();
  print_soinfo_offset<soinfoS>();
  print_soinfo_offset<soinfoT>();
  init_env();
  return RUN_ALL_TESTS();
}
