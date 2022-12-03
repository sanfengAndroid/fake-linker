#pragma once

#include <pthread.h>
#include <stddef.h>
#include <unordered_map>

#include <alog.h>

#include "linked_list.h"
#include "linker_block_allocator.h"
#include "linker_namespaces.h"
#include "linker_soinfo.h"

namespace fakelinker {

template <typename T, bool IsPointer = false>
struct SymbolItem {
  int min_api = __ANDROID_API_L__;
  int max_api = 100;
  const char *name = nullptr;
  T *pointer = nullptr;
  bool force = true;

  bool Set(gaddress addr) {
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

  bool SetValue(T *value) {
    if (value == nullptr) {
      return !force;
    }
    pointer = value;
    return true;
  }

  bool CheckApi(int api) { return api >= min_api && api < max_api; }
  T *Get() {
    if (!pointer) {
      LOGE("Attempt to get symbol/library `%s` address is empty, api is %d in [%d, %d), only to terminate program "
           "prevent error from sending",
           name, android_api, min_api, max_api);
      abort();
    }
    return pointer;
  }
};

template <typename T, bool IsPointer = false>
struct InternalSymbol : SymbolItem<T, IsPointer> {
  static constexpr int type = 0;
};

template <typename T, bool IsPointer = false>
struct ExportSymbol : SymbolItem<T, IsPointer> {
  static constexpr int type = 1;
};

struct LibrarySymbol : SymbolItem<soinfo, false> {
  static constexpr int type = 2;
};

struct LinkerSymbol {
  InternalSymbol<soinfo, true> solist;
  InternalSymbol<int> g_ld_debug_verbosity{{.force = false}};
  InternalSymbol<uint32_t> g_linker_logger{{.force = false}};
  InternalSymbol<pthread_mutex_t> g_dl_mutex;
  InternalSymbol<char> linker_dl_err_buf;
  InternalSymbol<void> link_image{{.force = false}};
  ANDROID_GE_N InternalSymbol<android_namespace_t> g_default_namespace{{.min_api = __ANDROID_API_N__}};
  ANDROID_LE_M InternalSymbol<std::unordered_map<uintptr_t, soinfo *>> g_soinfo_handles_map{
    {.min_api = __ANDROID_API_N__}};
  InternalSymbol<soinfo *> g_ld_preloads{{.max_api = __ANDROID_API_M__}};
  ExportSymbol<void *(const char *, int, const android_dlextinfo *)> dlopen{{.max_api = __ANDROID_API_N__}};
  InternalSymbol<void *(const char *, int, const android_dlextinfo *, const void *)> dlopenN{
    {.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__}
  };
  ExportSymbol<void *(const char *, int, const android_dlextinfo *, const void *)> dlopenO{
    {.min_api = __ANDROID_API_O__}};
  ExportSymbol<void *(void *, const char *)> dlsym{{.max_api = __ANDROID_API_N__}};
  /*
   * 在Android7中Linker导出 dlopen,
   * dlsym符号不包含caller地址,且不同设备可能内联了dlopen_ext和dlsym_impl符号,因此查找更底层内部符号do_dlopen,do_dlsym
   * 且还要修复dlerror
   * */
  InternalSymbol<bool(void *, const char *, const char *, void *, void **)> dlsymN{
    {.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__}
  };
  ExportSymbol<void *(void *, const char *, const void *)> dlsymO{{.min_api = __ANDROID_API_O__}};
  InternalSymbol<android_namespace_t *(const void *, const char *, const char *, const char *, uint64_t, const char *,
                                       android_namespace_t *)>
    create_namespaceN{
      {.min_api = __ANDROID_API_N__, .max_api = __ANDROID_API_O__}
  };
  ExportSymbol<android_namespace_t *(const char *, const char *, const char *, uint64_t, const char *,
                                     android_namespace_t *, const void *)>
    create_namespaceO{{.min_api = __ANDROID_API_O__}};

  InternalSymbol<LinkerTypeAllocator<soinfo>> g_soinfo_allocator;
  InternalSymbol<LinkerTypeAllocator<LinkedListEntry<soinfo>>> g_soinfo_links_allocator;
  InternalSymbol<LinkerTypeAllocator<android_namespace_t>> g_namespace_allocator{{.min_api = __ANDROID_API_N__}};
  InternalSymbol<LinkerTypeAllocator<LinkedListEntry<android_namespace_t>>> g_namespace_list_allocator{
    {.min_api = __ANDROID_API_N__}};

  LibrarySymbol linker_soinfo;

  void InitSymbolName();

  bool LoadSymbol();
};

extern LinkerSymbol linker_symbol;
} // namespace fakelinker