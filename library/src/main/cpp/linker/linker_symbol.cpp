#include "linker_symbol.h"

#include "elf_reader.h"
#include "linker_globals.h"
#if defined(__LP64__)
#define LP_TAIL "Pm"
#else
#define LP_TAIL "Pj"
#endif

namespace fakelinker {
LinkerSymbol linker_symbol;

void LinkerSymbol::InitSymbolName() {
  solist.name = "__dl__ZL6solist";
  g_ld_debug_verbosity.name = "__dl_g_ld_debug_verbosity";
  g_linker_logger.name = "__dl_g_linker_logger";
  g_dl_mutex.name = "__dl__ZL10g_dl_mutex";
  linker_dl_err_buf.name = "__dl__ZL19__linker_dl_err_buf";

  if (android_api >= __ANDROID_API_R__) {
    link_image.name = "__dl__ZN6soinfo10link_imageERK16SymbolLookupListPS_PK17android_"
                      "dlextinfo" LP_TAIL;
  } else if (android_api == __ANDROID_API_Q__) {
    link_image.name = "__dl__ZN6soinfo10link_imageERK10LinkedListIS_"
                      "19SoinfoListAllocatorES4_PK17android_dlextinfo" LP_TAIL;
  } else if (android_api >= __ANDROID_API_M__) {
    link_image.name = "__dl__ZN6soinfo10link_imageERK10LinkedListIS_"
                      "19SoinfoListAllocatorES4_PK17android_dlextinfo";
  } else if (android_api == __ANDROID_API_L_MR1__) {
    // todo 64位被内联,需要单独适配
    link_image.name = "__dl__ZN6soinfo9LinkImageEPK17android_dlextinfo";
  } else {
    link_image.name = "__dl__ZL17soinfo_link_imageP6soinfoPK17android_dlextinfo";
  }

  g_default_namespace.name = "__dl_g_default_namespace";


  if (android_api <= __ANDROID_API_N_MR1__) {
    g_soinfo_handles_map.name = "__dl__ZL20g_soinfo_handles_map";
  } else {
    g_soinfo_handles_map.name = "__dl_g_soinfo_handles_map";
  }

  g_ld_preloads.name = "__dl__ZL13g_ld_preloads";

  dlopen.name = "android_dlopen_ext";
  dlopenN.name = "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv";
  dlopenO.name = "__loader_android_dlopen_ext";

  dlsym.name = "dlsym";
  dlsymN.name = "__dl__Z8do_dlsymPvPKcS1_S_PS_";
  dlsymO.name = "__loader_dlsym";

  create_namespaceO.name = "__loader_android_create_namespace";

  create_namespaceN.name = is64BitBuild() ? "__dl__Z16create_namespacePKvPKcS2_S2_mS2_P19android_namespace_t"
                                          : "__dl__Z16create_namespacePKvPKcS2_S2_yS2_P19android_namespace_t";

  g_soinfo_allocator.name = "__dl__ZL18g_soinfo_allocator";
  g_soinfo_links_allocator.name = "__dl__ZL24g_soinfo_links_allocator";
  g_namespace_allocator.name = "__dl__ZL21g_namespace_allocator";
  g_namespace_list_allocator.name = "__dl__ZL26g_namespace_list_allocator";
}


#define APPEND_SYMBOL(var)                                                                                             \
  if (var.CheckApi(android_api)) {                                                                                     \
    if (var.name == nullptr) {                                                                                         \
      LOGE("Failed to check the symbol name `%s`, the current api `%d in [%d - %d)` requires a name but it is empty",  \
           #var, android_api, var.min_api, var.max_api);                                                               \
      return false;                                                                                                    \
    }                                                                                                                  \
    if constexpr (decltype(var)::type == 0) {                                                                          \
      internalSymbols.push_back(var.name);                                                                             \
    } else if constexpr (decltype(var)::type == 1) {                                                                   \
      exportSymbols.push_back(var.name);                                                                               \
    }                                                                                                                  \
  }

#define ASSIGN_SYMBOL(var)                                                                                             \
  if (var.CheckApi(android_api)) {                                                                                     \
    if constexpr (decltype(var)::type == 0) {                                                                          \
      if (!var.Set(internalAddresses[internal_index++])) {                                                             \
        LOGE("Failed to find Linker internal symbols '%s': %s", #var, var.name);                                       \
        return false;                                                                                                  \
      }                                                                                                                \
    } else if constexpr (decltype(var)::type == 1) {                                                                   \
      if (!var.Set(exportAddresses[export_index++])) {                                                                 \
        LOGE("Failed to find Linker export symbols '%s': %s", #var, var.name);                                         \
        return false;                                                                                                  \
      }                                                                                                                \
    }                                                                                                                  \
  }

bool LinkerSymbol::LoadSymbol() {
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

  std::vector<std::string> internalSymbols;
  std::vector<const char *> exportSymbols;
  APPEND_SYMBOL(solist);
  APPEND_SYMBOL(g_ld_debug_verbosity);
  APPEND_SYMBOL(g_linker_logger);
  APPEND_SYMBOL(g_dl_mutex);
  APPEND_SYMBOL(linker_dl_err_buf);
  APPEND_SYMBOL(link_image);
  APPEND_SYMBOL(g_default_namespace);
  APPEND_SYMBOL(g_soinfo_handles_map);
  APPEND_SYMBOL(g_ld_preloads);
  APPEND_SYMBOL(dlopen);
  APPEND_SYMBOL(dlopenN);
  APPEND_SYMBOL(dlopenO);
  APPEND_SYMBOL(dlsym);
  APPEND_SYMBOL(dlsymN);
  APPEND_SYMBOL(dlsymO);
  APPEND_SYMBOL(create_namespaceN);
  APPEND_SYMBOL(create_namespaceO);
  APPEND_SYMBOL(g_soinfo_allocator);
  APPEND_SYMBOL(g_soinfo_links_allocator);
  APPEND_SYMBOL(g_namespace_allocator);
  APPEND_SYMBOL(g_namespace_list_allocator);

  const char *library = is64BitBuild() ? "/linker64" : "/linker";

  std::vector<gaddress> internalAddresses;
  std::vector<gaddress> exportAddresses;

  if (!internalSymbols.empty()) {
    ElfReader reader;
    if (!reader.LoadFromDisk(library)) {
      return false;
    }
    internalAddresses = reader.FindInternalSymbols(internalSymbols);
    if (internalAddresses.size() != internalSymbols.size()) {
      LOGE("find linker internal symbols failed.");
      return false;
    }
    if (!solist.Set(internalAddresses[0])) {
      return false;
    }
  }
  if (!exportSymbols.empty()) {
    linker_soinfo =
      ProxyLinker::Get().FindSoinfoByName(android_api >= __ANDROID_API_O__ ? "ld-android.so" : "libdl.so");
    if (!linker_soinfo) {
      LOGE("find linker soinfo failed");
      return false;
    }
    for (auto name : exportSymbols) {
      exportAddresses.push_back(reinterpret_cast<gaddress>(linker_soinfo->find_export_symbol_address(name)));
    }
  }

  int internal_index = 0;
  int export_index = 0;

  ASSIGN_SYMBOL(solist);
  ASSIGN_SYMBOL(g_ld_debug_verbosity);
  ASSIGN_SYMBOL(g_linker_logger);
  ASSIGN_SYMBOL(g_dl_mutex);
  ASSIGN_SYMBOL(linker_dl_err_buf);
  ASSIGN_SYMBOL(link_image);
  ASSIGN_SYMBOL(g_default_namespace);
  ASSIGN_SYMBOL(g_soinfo_handles_map);
  ASSIGN_SYMBOL(g_ld_preloads);
  ASSIGN_SYMBOL(dlopen);
  ASSIGN_SYMBOL(dlopenN);
  ASSIGN_SYMBOL(dlopenO);
  ASSIGN_SYMBOL(dlsym);
  ASSIGN_SYMBOL(dlsymN);
  ASSIGN_SYMBOL(dlsymO);
  ASSIGN_SYMBOL(create_namespaceN);
  ASSIGN_SYMBOL(create_namespaceO);
  ASSIGN_SYMBOL(g_soinfo_allocator);
  ASSIGN_SYMBOL(g_soinfo_links_allocator);
  ASSIGN_SYMBOL(g_namespace_allocator);
  ASSIGN_SYMBOL(g_namespace_list_allocator);
  return true;
}

} // namespace fakelinker