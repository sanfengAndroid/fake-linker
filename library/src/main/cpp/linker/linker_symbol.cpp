#include "linker_symbol.h"

#include <android_level_compat.h>
#include <elf_reader.h>

#include "linker_globals.h"

#if defined(__LP64__)
#define LP_TAIL "Pm"
#else
#define LP_TAIL "Pj"
#endif

namespace fakelinker {
LinkerSymbol linker_symbol;

template <typename T, LinkerSymbolCategory Category, int Type, bool IsPointer, bool Required>
bool ProcessSymbol(LinkerSymbolCategory category,
                   std::function<uint64_t(const char *name, int symbol_type, bool find_prefix)> symbol_finder,
                   SymbolItem<T, Category, Type, IsPointer, Required> &symbol, bool find_prefix,
                   std::initializer_list<const char *> names) {
  // 按需加载不同类别符号
  if ((category & symbol.category) != symbol.category) {
    LOGD("skip target symbol %s category %d, required category %d", names.size() > 0 ? *names.begin() : "",
         symbol.category, category);
    return true;
  }
  if (names.size() == 0) {
    LOGD("skip target symbol no name");
    return true;
  }
  symbol.name = *names.begin();

  if (!symbol.CheckApi()) {
    LOGD("skip target symbol %s api level [%d, %d), current api level %d", names.size() > 0 ? *names.begin() : "",
         symbol.min_api, symbol.max_api, android_api);

    return true;
  }

  for (const char *name : names) {
    uint64_t address = symbol_finder(name, Type, false);
    if (address == 0 && find_prefix) {
      address = symbol_finder(name, Type, true);
    }
    if (address != 0) {
      symbol.Set(address);
      return true;
    }
  }
  bool res = !Required;
  if (!res) {
    symbol.name = *names.begin();
  }
  return res;
}

bool LinkerSymbol::LoadSymbol(LinkerSymbolCategory category) {
  const char *platform;
#if defined(__arm__)
  platform = "arm";
#elif defined(__aarch64__)
  platform = "arm64";
#elif defined(__i386__)
  platform = "x86";
#else
  platform = "x86_64";
#endif
  LOGE("Current operating platform: %s, api level: %d", platform, android_api);

  ElfReader reader;
  if (!reader.LoadFromDisk(is64BitBuild() ? "/linker64" : "/linker")) {
    return false;
  }
  if (!reader.CacheInternalSymbols()) {
    return false;
  }
  auto SymbolFinder = [&](const char *name, int symbol_type, bool find_prefix) -> uint64_t {
    if (symbol_type == InternalSymbol<void, kLinkerBase>::type) {
      return find_prefix ? reader.FindInternalSymbol(name) : reader.FindInternalSymbolByPrefix(name);
    }
    if (symbol_type == ExportSymbol<void, kLinkerBase>::type) {
      return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(linker_soinfo.Get()->find_export_symbol_address(name)));
    }
    if (symbol_type == LibrarySymbol<kLinkerBase>::type) {
      return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ProxyLinker::Get().FindSoinfoByName(name)));
    }
    LOGE("not supported find symbol type: %d", symbol_type);
    return 0;
  };
  bool find_prefix = android_api >= __ANDROID_API_U__;

#define PROCESS_SYMBOL(symbol, ...)                                                                                    \
  if (!ProcessSymbol(category, SymbolFinder, symbol, find_prefix, {__VA_ARGS__})) {                                    \
    LOGE("find linker %s symbol failed.", #symbol);                                                                    \
    return false;                                                                                                      \
  }

  PROCESS_SYMBOL(solist, "__dl__ZL6solist");
  PROCESS_SYMBOL(linker_soinfo, android_api >= __ANDROID_API_O__ ? "ld-android.so" : "libdl.so");

  PROCESS_SYMBOL(g_ld_debug_verbosity, "__dl_g_ld_debug_verbosity");
  PROCESS_SYMBOL(g_linker_debug_config, "__dl_g_linker_debug_config");
  PROCESS_SYMBOL(g_linker_logger, "__dl_g_linker_logger");

  if (android_api >= __ANDROID_API_U__) {
    PROCESS_SYMBOL(g_dl_mutex, "__dl_g_dl_mutex", "__dl__ZL10g_dl_mutex");
  } else {
    PROCESS_SYMBOL(g_dl_mutex, "__dl__ZL10g_dl_mutex");
  }
  PROCESS_SYMBOL(linker_dl_err_buf, "__dl__ZL19__linker_dl_err_buf");

  if (android_api >= __ANDROID_API_R__) {
    PROCESS_SYMBOL(link_image,
                   "__dl__ZN6soinfo10link_imageERK16SymbolLookupListPS_PK17android_"
                   "dlextinfo" LP_TAIL);
  } else if (android_api == __ANDROID_API_Q__) {
    PROCESS_SYMBOL(link_image,
                   "__dl__ZN6soinfo10link_imageERK10LinkedListIS_"
                   "19SoinfoListAllocatorES4_PK17android_dlextinfo" LP_TAIL);
  } else if (android_api >= __ANDROID_API_M__) {
    PROCESS_SYMBOL(link_image,
                   "__dl__ZN6soinfo10link_imageERK10LinkedListIS_"
                   "19SoinfoListAllocatorES4_PK17android_dlextinfo");
  } else if (android_api == __ANDROID_API_L_MR1__) {
    // todo Android 5.1 x86_64 该方法被内联了,需要单独适配
    PROCESS_SYMBOL(link_image, "__dl__ZN6soinfo9LinkImageEPK17android_dlextinfo");
  } else {
    PROCESS_SYMBOL(link_image, "__dl__ZL17soinfo_link_imageP6soinfoPK17android_dlextinfo");
  }
  PROCESS_SYMBOL(g_default_namespace, "__dl_g_default_namespace");
  if (android_api <= __ANDROID_API_N_MR1__) {
    PROCESS_SYMBOL(g_soinfo_handles_map, "__dl__ZL20g_soinfo_handles_map");
  } else {
    PROCESS_SYMBOL(g_soinfo_handles_map, "__dl_g_soinfo_handles_map");
  }
  PROCESS_SYMBOL(g_ld_preloads, "__dl__ZL13g_ld_preloads");
  PROCESS_SYMBOL(dlopen, "android_dlopen_ext");
  PROCESS_SYMBOL(dlopenN, "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv");
  PROCESS_SYMBOL(dlopenO, "__loader_android_dlopen_ext");
  PROCESS_SYMBOL(dlsym, "dlsym");
  PROCESS_SYMBOL(dlsymN, "__dl__Z8do_dlsymPvPKcS1_S_PS_");
  PROCESS_SYMBOL(dlsymO, "__loader_dlsym");
  PROCESS_SYMBOL(create_namespaceO, "__loader_android_create_namespace");
  PROCESS_SYMBOL(create_namespaceN,
                 is64BitBuild() ? "__dl__Z16create_namespacePKvPKcS2_S2_mS2_P19android_namespace_t"
                                : "__dl__Z16create_namespacePKvPKcS2_S2_yS2_P19android_namespace_t");

  PROCESS_SYMBOL(g_soinfo_allocator, "__dl__ZL18g_soinfo_allocator");
  PROCESS_SYMBOL(g_soinfo_links_allocator, "__dl__ZL24g_soinfo_links_allocator");
  PROCESS_SYMBOL(g_namespace_allocator, "__dl__ZL21g_namespace_allocator");
  PROCESS_SYMBOL(g_namespace_list_allocator, "__dl__ZL26g_namespace_list_allocator");
  return true;
}

} // namespace fakelinker