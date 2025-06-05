#pragma once

#include <android/log.h>
#include <pthread.h>
#include <stddef.h>

#include <unordered_map>

#include "linked_list.h"
#include "linker_block_allocator.h"
#include "linker_namespaces.h"
#include "linker_soinfo.h"
#include "linker_tls.h"

namespace fakelinker {

/**
 * @brief Categorize symbols - sometimes only individual symbols are used, so we don't need to load all symbols
 *
 */
enum LinkerSymbolCategory {
  // Most basic linker symbols, must be found
  kLinkerBase = 0,
  // Linker debug logging related
  kLinkerDebug = 1,
  // dlopen/dlsym related
  kDlopenDlSym = 1 << 1,
  // soinfo namespace related
  kNamespace = 1 << 2,
  // soinfo handler related for Android 7.0+
  kSoinfoHandler = 1 << 3,
  // Load and modify soinfo related memory protection symbols
  kSoinfoMemory = 1 << 4,
  // Load all symbols
  kLinkerAll = 0xFFFFFFFF,
};

template <typename T, LinkerSymbolCategory Category, int Type, bool IsPointer = false, bool Required = false>
struct SymbolItem {
  static constexpr int type = Type;
  int min_api = __ANDROID_API_L__;
  int max_api = 100;
  const char *name = nullptr;
  T *pointer = nullptr;
  bool force = true;
  LinkerSymbolCategory category = Category;

  bool Set(Address addr) {
    if (addr == 0) {
      return !force;
    }
    if (IsPointer) {
      pointer = *reinterpret_cast<T **>(addr);
    } else {
      pointer = reinterpret_cast<T *>(addr);
    }
    return pointer != nullptr;
  }

  bool CheckApi() { return android_api >= min_api && android_api < max_api; }

  T *Get() {
    if (!CheckApi()) {
      LOGW("Warning: API level %d constraints [%d, %d) not met, please check the calling code", android_api, min_api,
           max_api);
      return nullptr;
    }

    if (!pointer && force) {
      LOGE("Attempt to get symbol/library `%s` address is empty, api is %d in [%d, %d), only to terminate program "
           "prevent error from sending",
           name, android_api, min_api, max_api);
      abort();
    }
    return pointer;
  }
};

template <typename T, LinkerSymbolCategory Category, bool IsPointer = false, bool Required = false>
using InternalSymbol = SymbolItem<T, Category, 0, IsPointer, Required>;


template <typename T, LinkerSymbolCategory Category, bool IsPointer = false, bool Required = false>
using ExportSymbol = SymbolItem<T, Category, 1, IsPointer, Required>;

template <LinkerSymbolCategory Category>
using LibrarySymbol = SymbolItem<soinfo, Category, 2, false, true>;

ANDROID_GE_V struct LinkerDebugConfig {
  // Set automatically if any of the more specific options are set.
  bool any;

  // Messages relating to calling ctors/dtors/ifuncs.
  bool calls;
  // Messages relating to CFI.
  bool cfi;
  // Messages relating to the dynamic section.
  bool dynamic;
  // Messages relating to symbol lookup.
  bool lookup;
  // Messages relating to relocation processing.
  bool reloc;
  // Messages relating to ELF properties.
  bool props;
  // TODO: "config" and "zip" seem likely to want to be separate?

  bool timing;
  bool statistics;
};

struct LinkerSymbol {
  InternalSymbol<soinfo, kLinkerBase, true, true> solist;
  ANDROID_LE_U InternalSymbol<int, kLinkerDebug> g_ld_debug_verbosity{.min_api = __ANDROID_API_U__};
  ANDROID_GE_U InternalSymbol<LinkerDebugConfig, kLinkerDebug> g_linker_debug_config{.min_api = __ANDROID_API_U__};
  InternalSymbol<uint32_t, kLinkerDebug> g_linker_logger;

  InternalSymbol<pthread_mutex_t, kDlopenDlSym> g_dl_mutex;
  InternalSymbol<char, kDlopenDlSym> linker_dl_err_buf;
  InternalSymbol<void, kDlopenDlSym> link_image;
  ANDROID_GE_N InternalSymbol<android_namespace_t, kNamespace> g_default_namespace{.min_api = __ANDROID_API_N__};
  ANDROID_GE_N InternalSymbol<std::unordered_map<uintptr_t, soinfo *>, kSoinfoHandler> g_soinfo_handles_map{
    .min_api = __ANDROID_API_N__};
  ANDROID_LE_M InternalSymbol<soinfo *, kDlopenDlSym> g_ld_preloads{.max_api = __ANDROID_API_M__};

  ANDROID_LE_M ExportSymbol<void *(const char *, int, const android_dlextinfo *), kDlopenDlSym> dlopen{
    .max_api = __ANDROID_API_N__};
  ANDROID_LE_N1 ANDROID_GE_N
    InternalSymbol<void *(const char *, int, const android_dlextinfo *, const void *), kDlopenDlSym>
      dlopenN{.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__};
  ANDROID_GE_O ExportSymbol<void *(const char *, int, const android_dlextinfo *, const void *), kDlopenDlSym> dlopenO{
    .min_api = __ANDROID_API_O__};

  ANDROID_LE_M ExportSymbol<void *(void *, const char *), kDlopenDlSym> dlsym{.max_api = __ANDROID_API_N__};
  /*
   * In Android 7, Linker exports dlopen,
   * dlsym symbol does not include caller address, and different devices may have inlined dlopen_ext and dlsym_impl
   * symbols, therefore we need to find lower-level internal symbols do_dlopen, do_dlsym and also need to fix dlerror
   */
  ANDROID_LE_N1 ANDROID_GE_N InternalSymbol<bool(void *, const char *, const char *, void *, void **), kDlopenDlSym>
    dlsymN{.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__};
  ANDROID_GE_O ExportSymbol<void *(void *, const char *, const void *), kDlopenDlSym> dlsymO{.min_api =
                                                                                               __ANDROID_API_O__};

  ANDROID_LE_N1 ANDROID_GE_N
    InternalSymbol<android_namespace_t *(const void *, const char *, const char *, const char *, uint64_t, const char *,
                                         android_namespace_t *),
                   kNamespace>
      create_namespaceN{.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__};
  ANDROID_GE_O ExportSymbol<android_namespace_t *(const char *, const char *, const char *, uint64_t, const char *,
                                                  android_namespace_t *, const void *),
                            kNamespace>
    create_namespaceO{.min_api = __ANDROID_API_O__};

  InternalSymbol<LinkerTypeAllocator<soinfo>, kSoinfoMemory> g_soinfo_allocator;
  InternalSymbol<LinkerTypeAllocator<LinkedListEntry<soinfo>>, kSoinfoMemory> g_soinfo_links_allocator;
  ANDROID_GE_N InternalSymbol<LinkerTypeAllocator<android_namespace_t>, kSoinfoMemory> g_namespace_allocator{
    .min_api = __ANDROID_API_N__};
  ANDROID_GE_N InternalSymbol<LinkerTypeAllocator<LinkedListEntry<android_namespace_t>>, kSoinfoMemory>
    g_namespace_list_allocator{.min_api = __ANDROID_API_N__};

  LibrarySymbol<kDlopenDlSym> linker_soinfo;

  bool LoadSymbol(LinkerSymbolCategory category = kLinkerAll);
};

extern LinkerSymbol linker_symbol;
} // namespace fakelinker