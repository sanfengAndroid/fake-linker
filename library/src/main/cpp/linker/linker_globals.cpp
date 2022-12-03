
//
// Created by beich on 2020/11/5.
//

/*
 * Android 5.0 x86
 * soinfo_link_image函数被优化不遵循cdecl调用约定，因此无法使用系统重定位
 * Android 5.1 x86_64 LinkImage 被内联了
 * */
#include "linker_globals.h"

#include <algorithm>
#include <dlfcn.h>
#include <elf.h>
#include <sys/mman.h>

#include <alog.h>
#include <fake_linker.h>
#include <macros.h>
#include <maps_util.h>

#include "bionic/get_tls.h"
#include "elf_reader.h"
#include "linker_namespaces.h"
#include "linker_soinfo.h"
#include "linker_symbol.h"
#include "linker_util.h"
#include "scoped_pthread_mutex_locker.h"

#define DEFAULT_NAMESPACE_NAME "(default)"

#if defined(__LP64__)
#define LP_TAIL "Pm"
#else
#define LP_TAIL "Pj"
#endif

namespace fakelinker {

/*
 * 关于dlopen dlsym
 * Android 8.0 以上 linker导出符号 __loader_dlopen, 0__loader_dlsym
 * 可以直接修改caller地址 Android 7.0 ~ Android 7.1.2 linker 中导出符号 dlopen,
 * dlsym ,其可能内联了dlopen_ext,dlsym_impl函数因此直接调用无法修改
 *  而内联了则要查找do_dlopen,do_dlsym函数,而当失败时还要修复dlerror,且__bionic_format_dlerror也被内联了
 * Android 7.0以下直接使用原生dlopen和dlsym即可
 * */

/*
 * Android 11
 * lookup_list包含所有可以访问的全局组和本地组, local_group_root this
 * bool soinfo::link_image(const SymbolLookupList& lookup_list, soinfo*
 local_group_root, const android_dlextinfo* extinfo, size_t* relro_fd_offset)
 * Android 10
 * bool soinfo::link_image(const soinfo_list_t& global_group, const
 soinfo_list_t& local_group, const android_dlextinfo* extinfo, size_t*
 relro_fd_offset)
 * Android 9, 8.1, 8,7.1,7,6.0
 *  bool soinfo::link_image(const soinfo_list_t& global_group, const
 soinfo_list_t& local_group, const android_dlextinfo* extinfo)
 * Android 5.1
 * bool soinfo::LinkImage(const android_dlextinfo* extinfo);
 * Android 5.0 static bool soinfo_link_image(soinfo* si, const
 android_dlextinfo* extinfo)
 * */

typedef struct {
  const char *name;
  int index;
  gaddress old_address;
  gaddress new_address;
} symbol_observed;

enum walk_action_result_t : uint32_t { kWalkStop = 0, kWalkContinue = 1, kWalkSkip = 2 };

ANDROID_GE_N soinfo *ProxyLinker::SoinfoFromHandle(const void *handle) {
  if ((reinterpret_cast<uintptr_t>(handle) & 1) != 0) {
    auto it = linker_symbol.g_soinfo_handles_map.Get()->find(reinterpret_cast<uintptr_t>(handle));
    if (it == linker_symbol.g_soinfo_handles_map.Get()->end()) {
      return nullptr;
    } else {
      return it->second;
    }
  }
  return static_cast<soinfo *>(const_cast<void *>(handle));
}

ANDROID_GE_N std::vector<android_namespace_t *> ProxyLinker::GetAllNamespace(bool flush) {
  if (!namespaces.empty() && !flush) {
    return namespaces;
  }
  soinfo *si = linker_symbol.solist.Get();
  do {
    auto find = std::find_if(namespaces.begin(), namespaces.end(), [&](android_namespace_t *t) {
      return t == si->get_primary_namespace();
    });
    if (find == namespaces.end()) {
      namespaces.push_back(si->get_primary_namespace());
    }
  } while ((si = si->next()) != nullptr);
  return namespaces;
}

android_namespace_t *ProxyLinker::FindNamespaceByName(const char *name) {
  if (name == nullptr) {
    return nullptr;
  }
  for (const auto &np : ProxyLinker::GetAllNamespace(true)) {
    if (np->get_name() != nullptr && strcmp(np->get_name(), name) == 0) {
      return np;
    }
  }
  return nullptr;
}

android_namespace_t *ProxyLinker::GetDefaultNamespace() { return linker_symbol.g_default_namespace.Get(); }

bool ProxyLinker::AddGlobalSoinfoToNamespace(soinfo *global, android_namespace_t *np) {
  if (__predict_false(global == nullptr) || __predict_false(np == nullptr)) {
    return false;
  }
  LinkerBlockLock lock;
  global->set_dt_flags_1(global->dt_flags_1() | DF_1_GLOBAL);
  np->add_soinfo(global);
  global->add_secondary_namespace(np);
  return true;
}

ANDROID_GE_N bool ProxyLinker::RemoveSoinfoFromNamespcae(soinfo *so, android_namespace_t *np, bool clear_global_flags) {
  if (__predict_false(so == nullptr) || __predict_false(np == nullptr)) {
    return false;
  }
  LinkerBlockLock lock;
  np->remove_soinfo(so);
  if (IsGlobalSoinfo(so)) {
    if (clear_global_flags) {
      so->remove_all_secondary_namespace();
      so->set_dt_flags_1(so->dt_flags_1() & ~DF_1_GLOBAL);
    } else {
      so->remove_secondary_namespace(np);
    }
  }
  return true;
}

void ProxyLinker::ChangeSoinfoOfNamespace(soinfo *so, android_namespace_t *np) {
  if (np == nullptr) {
    ProxyLinker::AddSoinfoToDefaultNamespace(so);
    return;
  }
  if (so == nullptr) {
    return;
  }
  LinkerBlockLock lock;
  so->get_primary_namespace()->remove_soinfo(so);
  so->get_primary_namespace() = np;
  np->add_soinfo(so);
}

void ProxyLinker::AddSoinfoToDefaultNamespace(soinfo *si) {
  if (si == nullptr) {
    return;
  }
  if (strcmp(si->get_primary_namespace()->get_name(), DEFAULT_NAMESPACE_NAME) == 0) {
    return;
  }
  LinkerBlockLock lock;
  for (auto &np : ProxyLinker::GetAllNamespace(true)) {
    if (strcmp(np->get_name(), DEFAULT_NAMESPACE_NAME) == 0) {
      si->get_primary_namespace()->remove_soinfo(si);
      si->get_primary_namespace() = np;
      np->add_soinfo(si);
      break;
    }
  }
}

ANDROID_GE_N void *ProxyLinker::CallDoDlopenN(const char *filename, int flags, const android_dlextinfo *extinfo,
                                              void *caller_addr) {
  if (android_api >= __ANDROID_API_O__) {
    return linker_symbol.dlopenO.Get()(filename, flags, extinfo, caller_addr);
  }
  ScopedPthreadMutexLocker locker(linker_symbol.g_dl_mutex.Get());
  void *result = linker_symbol.dlopenN.Get()(filename, flags, extinfo, caller_addr);
  if (result == nullptr) {
    FormatDlerror("dlopen failed", linker_symbol.linker_dl_err_buf.Get());
  }
  return result;
}

ANDROID_GE_N bool ProxyLinker::CallDoDlsymN(void *handle, const char *symbol, const char *version, void *caller_addr,
                                            void **sym) {
  if (android_api >= __ANDROID_API_O__) {
    *sym = linker_symbol.dlsymO.Get()(handle, symbol, caller_addr);
    return *sym != nullptr;
  }
  // 符号没找到要设置错误输出,避免后续调用dlerror出错
  ScopedPthreadMutexLocker locker(linker_symbol.g_dl_mutex.Get());
  bool found = linker_symbol.dlsymN.Get()(handle, symbol, version, caller_addr, sym);
  if (!found) {
    FormatDlerror(linker_symbol.linker_dl_err_buf.Get(), nullptr);
  }
  return found;
}

ANDROID_GE_N android_namespace_t *ProxyLinker::CallCreateNamespace(const char *name, const char *ld_library_path,
                                                                   const char *default_library_path, uint64_t type,
                                                                   const char *permitted_when_isolated_path,
                                                                   android_namespace_t *parent_namespace,
                                                                   const void *caller_addr) {
  LinkerBlockLock lock;
  if (android_api >= __ANDROID_API_O__) {
    return linker_symbol.create_namespaceO.Get()(name, ld_library_path, default_library_path, type,
                                                 permitted_when_isolated_path, parent_namespace, caller_addr);
  }
  ScopedPthreadMutexLocker locker(linker_symbol.g_dl_mutex.Get());
  android_namespace_t *result = linker_symbol.create_namespaceN.Get()(
    caller_addr, name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_namespace);
  if (result == nullptr) {
    FormatDlerror("android_create_namespace failed", linker_symbol.linker_dl_err_buf.Get());
  }
  return result;
}

ANDROID_LE_M void *ProxyLinker::CallDoDlopen(const char *name, int flags, const android_dlextinfo *extinfo) {
  return linker_symbol.dlopen.Get()(name, flags, extinfo);
}

ANDROID_LE_M void *ProxyLinker::CallDoDlsym(void *handle, const char *symbol) { return dlsym(handle, symbol); }

/*
 * pthread_internal_t->dlerror_buffer
 *
 * api 21, 22
 *      x86: 68, x86_64:168
 * api 23
 *      x86: 1220, x86_64:2472
 * api 24, 25
 *      x86,arm: 1244, x86_64,arm64: 2480
 * api 26,27,28
 *      x86,arm: 1156, x86_64,arm64:2304
 * api 29
 *      x86,arm: 92, x86_64,arm64: 176
 * api 30
 *      x86: 144, x86_64: 240
 *
 * dlerror_slot  TLS_SLOT_DLERROR = 6
 * api 30      pthread_internal_t->current_dlerror
 *      x86: 140, x86_64: 232
 * api 29      pthread_internal_t->current_dlerror
 *       x86,arm: 88, x86_64,arm64: 168
 * api 21 ~ 28  __get_tls[TLS_SLOT_DLERROR]
 *
 *
 * */
void ProxyLinker::FormatDlerror(const char *msg, const char *detail) {
  int dlerrorBufferOffset;

  if (android_api >= __ANDROID_API_R__) {
    dlerrorBufferOffset = is64BitBuild() ? 240 : 144;
  } else if (android_api == __ANDROID_API_Q__) {
    dlerrorBufferOffset = is64BitBuild() ? 176 : 92;
  } else if (android_api >= __ANDROID_API_O__) {
    dlerrorBufferOffset = is64BitBuild() ? 2304 : 1156;
  } else if (android_api >= __ANDROID_API_N__) {
    dlerrorBufferOffset = is64BitBuild() ? 2480 : 1244;
  } else if (android_api >= __ANDROID_API_M__) {
    dlerrorBufferOffset = is64BitBuild() ? 2472 : 1220;
  } else {
    dlerrorBufferOffset = is64BitBuild() ? 168 : 68;
  }


#define DLERROR_BUFFER_SIZE 512
  void *pthread_internal_t_ptr = GetTls()[1];
  char *buffer = (char *)pthread_internal_t_ptr + dlerrorBufferOffset;
  strlcpy(buffer, msg, DLERROR_BUFFER_SIZE);
  if (detail != nullptr) {
    strlcat(buffer, ": ", DLERROR_BUFFER_SIZE);
    strlcat(buffer, detail, DLERROR_BUFFER_SIZE);
  }
  if (android_api >= __ANDROID_API_Q__) {
    char *current_dlerror = buffer - sizeof(void *);
    current_dlerror = buffer;
  } else {
    char **dlerror_slot = reinterpret_cast<char **>(&GetTls()[6]);
    *dlerror_slot = buffer;
  }
#undef DLERROR_BUFFER_SIZE
}

void **ProxyLinker::GetTls() { return __get_tls(); }

soinfo *ProxyLinker::FindSoinfoByName(const char *name) {
  soinfo *si = linker_symbol.solist.Get();
  int len_a = strlen(name);
  do {
    if (const char *so_name = si->get_soname()) {
      // 低版本保存了完整路径,因此这里只判断结尾
      int delta = strlen(so_name) - len_a;
      if (delta == 0 && strncmp(so_name, name, len_a) == 0) {
        return si;

      } else if (delta > 0 && strncmp(so_name + delta, name, len_a) == 0 && so_name[delta - 1] == '/') {
        return si;
      }
    }
  } while ((si = si->next()) != nullptr);
  return nullptr;
}

soinfo *ProxyLinker::FindSoinfoByPath(const char *path) {
  soinfo *si = linker_symbol.solist.Get();
  do {
    if (si->get_soname() != nullptr) {
      if (strstr(si->realpath(), path) != nullptr) {
        return si;
      }
    }
  } while ((si = si->next()) != nullptr);
  return nullptr;
}

std::vector<soinfo *> ProxyLinker::GetAllSoinfo() {
  std::vector<soinfo *> vec;
  vec.reserve(20);
  soinfo *si = linker_symbol.solist.Get();
  do {
    vec.push_back(si);
  } while ((si = si->next()) != nullptr);
  return vec;
}

soinfo *ProxyLinker::FindContainingLibrary(const void *p) {
  soinfo *si = linker_symbol.solist.Get();
  ElfW(Addr) address = reinterpret_cast<ElfW(Addr)>(untag_address(p));
  do {
    if (address >= si->base() && address - si->base() < si->size()) {
      return si;
    }
  } while ((si = si->next()) != nullptr);
  return nullptr;
}

soinfo *ProxyLinker::GetLinkerSoinfo() { return linker_symbol.linker_soinfo.Get(); }

void *ProxyLinker::FindSymbolByDlsym(soinfo *si, const char *name) {
  void *result;
  if (android_api >= __ANDROID_API_N__) {
    bool success = CallDoDlsymN(reinterpret_cast<void *>(si->get_handle()), name, nullptr,
                                reinterpret_cast<void *>(si->load_bias()), &result);
    result = success ? result : nullptr;
  } else {
    result = CallDoDlsym(si, name);
  }
  return result;
}

template <typename F>
static bool walk_dependencies_tree(soinfo *root_soinfo, F action) {
  SoinfoLinkedList visit_list;
  SoinfoLinkedList visited;

  visit_list.push_back(root_soinfo);

  soinfo *si;
  while ((si = visit_list.pop_front()) != nullptr) {
    if (visited.contains(si)) {
      continue;
    }

    walk_action_result_t result = action(si);
    if (result == kWalkStop) {
      return false;
    }
    visited.push_back(si);
    if (result != kWalkSkip) {
      si->get_children().for_each([&](soinfo *child) {
        visit_list.push_back(child);
      });
    }
  }
  return true;
}

/*
 * Android 7.0以下获取全局库
 * */
soinfo_list_t ProxyLinker::GetGlobalGroupM() {
  soinfo_list_t global_group;
  for (soinfo *si = linker_symbol.solist.Get(); si != nullptr; si = si->next()) {
    if ((si->dt_flags_1() & DF_1_GLOBAL) != 0) {
      global_group.push_back(si);
    }
  }
  return global_group;
}

soinfo_list_t ProxyLinker::GetSoinfoGlobalGroup(soinfo *root) {
  if (android_api >= __ANDROID_API_N__) {
    return root->get_primary_namespace()->get_global_group();
  } else {
    return GetGlobalGroupM();
  }
}

static soinfo_list_t GetSoinfoLocalGroup(soinfo *root) {
  soinfo_list_t local_group;
  if (android_api >= __ANDROID_API_N__) {
    android_namespace_t *local_group_ns = root->get_primary_namespace();
    walk_dependencies_tree(root, [&](soinfo *child) {
      if (local_group_ns->is_accessible(child)) {
        local_group.push_back(child);
        return kWalkContinue;
      } else {
        return kWalkSkip;
      }
    });
  } else {
    walk_dependencies_tree(root, [&](soinfo *child) {
      local_group.push_back(child);
      return kWalkContinue;
    });
  }
  return local_group;
}

/*
 * 5.0 ~ 6.0 把soinfo添加进 g_ld_preloads
 * 6.0+ 添加DF_1_GLOBAL标志
 * 7.0+ 需要将 soinfo 添加到所有命名空间
 *
 * Android 7.0以下添加进全局组后拥有
 * RTLD_GLOBAL标志,这会导致在全局库中使用dlsym无法查找到符号
 * */
void ProxyLinker::AddSoinfoToGlobal(soinfo *si) {
  if (si == nullptr) {
    return;
  }
  LinkerBlockLock lock;
  if (android_api >= __ANDROID_API_M__) {
    si->set_dt_flags_1(si->dt_flags_1() | DF_1_GLOBAL);
    if (android_api >= __ANDROID_API_N__) {
      // void *main_handle = dlopen(nullptr, 0);
      // ProxyLinker::ChangeSoinfoOfNamespace(si,ProxyLinker::SoinfoFromHandle(main_handle)->get_primary_namespace());
      for (const auto np : ProxyLinker::GetAllNamespace(true)) {
        if (si->get_primary_namespace() != np) {
          np->add_soinfo(si);
          si->add_secondary_namespace(np);
        }
      }
    }
  } else {
    for (int i = 0; i < 9; ++i) {
      if (linker_symbol.g_ld_preloads.Get()[i] == nullptr) {
        linker_symbol.g_ld_preloads.Get()[i] = si;
        break;
      }
    }
  }
}

bool ProxyLinker::RemoveGlobalSoinfo(soinfo *si) {
  if (si == nullptr) {
    return false;
  }
  LinkerBlockLock lock;
  if (android_api >= __ANDROID_API_M__) {
    si->set_dt_flags_1(si->dt_flags_1() & ~DF_1_GLOBAL);
    if (android_api >= __ANDROID_API_N__) {
      for (const auto np : ProxyLinker::GetAllNamespace(true)) {
        if (si->get_primary_namespace() != np) {
          np->remove_soinfo(si);
          si->remove_secondary_namespace(np);
        }
      }
    }
  } else {
    int index = -1;
    soinfo **preloads = linker_symbol.g_ld_preloads.Get();
    for (int i = 0; i < 9; ++i) {
      if (preloads[i] == nullptr) {
        break;
      }
      if (preloads[i] == si) {
        preloads[i] = nullptr;
        index = i;
      } else if (index >= 0 && index + 1 == i) {
        preloads[index] = preloads[i];
        index++;
      }
    }
  }
  return true;
}

bool ProxyLinker::IsGlobalSoinfo(soinfo *si) {
  if (si == nullptr) {
    return false;
  }
  if (android_api >= __ANDROID_API_M__) {
    return (si->dt_flags_1() & DF_1_GLOBAL) == DF_1_GLOBAL;
  }

  for (int i = 0; i < 9; ++i) {
    if (linker_symbol.g_ld_preloads.Get()[i] == si) {
      return true;
    }
  }
  return false;
}

int *ProxyLinker::GetGLdDebugVerbosity() { return linker_symbol.g_ld_debug_verbosity.Get(); }

bool ProxyLinker::SetLdDebugVerbosity(int level) {
  if (int *p = linker_symbol.g_ld_debug_verbosity.Get()) {
    *p = level;
    return true;
  }
  return false;
}

/*
 * 6.0以下重写链接
 * */
ANDROID_LE_L1 bool ProxyLinker::RelinkSoinfoImplL(soinfo *si) {
  //	bool soinfo::LinkImage(const android_dlextinfo* extinfo);
  //	static bool soinfo_link_image(soinfo* si, const android_dlextinfo*
  // extinfo)
  return reinterpret_cast<bool (*)(soinfo *, const android_dlextinfo *)>(linker_symbol.link_image.Get())(si, nullptr);
}

/* RelinkSoinfoImplM
 * 6.0及以上重新链接
 * */
ANDROID_GE_M bool ProxyLinker::RelinkSoinfoImplM(soinfo *si) {
  if (android_api < __ANDROID_API_M__) {
    return false;
  }
  bool linked = false;
  soinfo_list_t local_group = GetSoinfoLocalGroup(si);
  soinfo_list_t global_group = GetSoinfoGlobalGroup(si);
  linked = local_group.visit([&](soinfo *lib) {
    if (lib->is_linked()) {
      return true;
    }
    if (android_api >= __ANDROID_API_N__ && lib->get_primary_namespace() != si->get_primary_namespace()) {
      return false;
    }
    // api >= __ANDROID_API_Q__
    const android_dlextinfo *link_extinfo = nullptr;
    size_t relro_fd_offset = 0;

    if (android_api >= __ANDROID_API_R__) {
      // bool soinfo::link_image(const SymbolLookupList& lookup_list,
      //                         soinfo* local_group_root,
      //						const android_dlextinfo*
      // extinfo, 	    				size_t* relro_fd_offset)
      SymbolLookupList lookup_list(global_group, local_group);
      soinfo *local_group_root = local_group.front();
      lookup_list.set_dt_symbolic_lib(lib->has_DT_SYMBOLIC() ? lib : nullptr);
      return reinterpret_cast<bool (*)(soinfo *, const SymbolLookupList &, soinfo *, const android_dlextinfo *,
                                       size_t *)>(linker_symbol.link_image.Get())(lib, lookup_list, local_group_root,
                                                                                  link_extinfo, &relro_fd_offset);
    }

    if (android_api == __ANDROID_API_Q__) {
      //	bool soinfo::link_image(const soinfo_list_t& global_group,
      //                        	const soinfo_list_t& local_group,
      //                            const android_dlextinfo* extinfo,
      //                            size_t* relro_fd_offset)
      return reinterpret_cast<bool (*)(soinfo *, const soinfo_list_t &, const soinfo_list_t &,
                                       const android_dlextinfo *, size_t *)>(linker_symbol.link_image.Get())(
        lib, global_group, local_group, link_extinfo, &relro_fd_offset);
    }
    //	bool soinfo::link_image(const soinfo_list_t &global_group,
    //                      	const soinfo_list_t &local_group,
    //							const android_dlextinfo
    //*extinfo)
    return reinterpret_cast<bool (*)(soinfo *, const soinfo_list_t &, const soinfo_list_t &,
                                     const android_dlextinfo *)>(linker_symbol.link_image.Get())(lib, global_group,
                                                                                                 local_group, nullptr);
  });
  return linked;
}

static int unprotect_rel_data(gaddress start, gaddress end, int port) {
  int error = mprotect(reinterpret_cast<void *>(start), end - start, port);
  if (error < 0) {
    LOGE("unprotect rel offset error: %d, port: %d ", error, port);
  }
  return error;
}

static void symbol_detect(soinfo *si, symbol_observed *symbols, size_t len, bool first) {
#ifdef USE_RELA
  ElfW(Rela) *start = si->plt_rela() == nullptr ? si->rela() : si->plt_rela();
  ElfW(Rela) *end = si->plt_rela() == nullptr ? si->rela() + si->rela_count() : si->plt_rela() + si->plt_rela_count();
#else
  ElfW(Rel) *start = si->plt_rel() == nullptr ? si->rel() : si->plt_rel();
  ElfW(Rel) *end = si->plt_rel() == nullptr ? si->rel() + si->rel_count() : si->plt_rel() + si->plt_rel_count();
#endif
  if (first) {
    int index = 0;
    for (; start < end; start++, index++) {
      const char *symbol_name = si->strtab() + (si->symtab() + R_SYM(start->r_info))->st_name;
      for (int i = 0; i < len; ++i) {
        if (symbols[i].index == 0 && strcmp(symbols[i].name, symbol_name) == 0) {
          symbols[i].index = index;
          symbols[i].old_address = *(reinterpret_cast<gsize *>(start->r_offset + si->load_bias()));
          break;
        }
      }
    }
  } else {
    for (int i = 0; i < len; ++i) {
      if (symbols[i].index == 0) {
        continue;
      }
      symbols[i].new_address = *reinterpret_cast<gsize *>((start + symbols[i].index)->r_offset + si->load_bias());
    }
  }
}

bool ProxyLinker::ManualRelinkLibraries(soinfo *global, const std::vector<std::string> &sonames,
                                        const std::vector<std::string> &filters) {
  if (__predict_false(global == nullptr) || __predict_false(sonames.empty())) {
    return false;
  }
  symbol_relocations rels = global->get_global_soinfo_export_symbols(filters);
  if (rels.empty()) {
    LOGW("Function symbols not exported by the global library : %s",
         global->get_soname() == nullptr ? "(null)" : global->get_soname());
    return false;
  }
  bool success = true;
  for (size_t i = 0, e = sonames.size(); i < e; ++i) {
    soinfo *child = ProxyLinker::FindSoinfoByName(sonames[i].c_str());
    if (child == nullptr) {
      LOGW("The specified so was not found: %s", sonames[i].c_str());
    } else {
      success &= ProxyLinker::ManualRelinkLibrary(rels, child);
    }
  }
  return success;
}

bool ProxyLinker::ManualRelinkLibraries(soinfo *global, int len, const std::vector<soinfo *> &targets,
                                        std::vector<std::string> &filters) {
  if (__predict_false(global == nullptr) || __predict_false(targets.empty())) {
    return false;
  }
  symbol_relocations rels = global->get_global_soinfo_export_symbols(filters);
  if (rels.empty()) {
    LOGW("Function symbols not exported by the global library : %s",
         global->get_soname() == nullptr ? "(null)" : global->get_soname());
    return false;
  }
  bool success = true;
  for (int i = 0; i < len; ++i) {
    success &= ProxyLinker::ManualRelinkLibrary(rels, targets[i]);
  }
  return success;
}

bool ProxyLinker::ManualRelinkLibrary(soinfo *global, soinfo *child) {
  std::vector<std::string> filters;
  return ProxyLinker::ManualRelinkLibrary(global, child, filters);
}

bool ProxyLinker::ManualRelinkLibrary(soinfo *global, soinfo *child, std::vector<std::string> &filters) {
  if (__predict_false(global == nullptr)) {
    return false;
  }
  symbol_relocations rels = global->get_global_soinfo_export_symbols(filters);
  if (rels.empty()) {
    LOGW("Function symbols not exported by the global library : %s",
         global->get_soname() == nullptr ? "(null)" : global->get_soname());
    return false;
  }
  return ProxyLinker::ManualRelinkLibrary(rels, child);
}

bool ProxyLinker::ManualRelinkLibrary(symbol_relocations &rels, soinfo *child) {
  if (rels.empty() || __predict_false(child == nullptr)) {
    return false;
  }
  ScopedPthreadMutexLocker locker(linker_symbol.g_dl_mutex.Get());
  return child->again_process_relocation(rels);
}

/*
 * 调用系统重定位会出现各种问题,废弃使用
 *
 * */
bool ProxyLinker::SystemRelinkLibrary(soinfo *so) {
  bool success;
  if (so == nullptr) {
    return false;
  }
  if (linker_symbol.link_image.Get() == nullptr) {
    LOGE("This device does not support the system relink, not found link_image "
         "impl function");
    return false;
  }
  ScopedPthreadMutexLocker locker(linker_symbol.g_dl_mutex.Get());
  LinkerBlockLock lock;
  so->set_unlinked();
  // 重新dlopen出错,因为目前进程已经存在该so就不会在走ElfRead
  // 5.0查找会修改linker的数据段,因此还要解保护linker
  MapsHelper util(so->realpath());
  if (!util) {
    return false;
  }
  util.UnlockPageProtect();
  if (android_api < __ANDROID_API_M__) {
    success = RelinkSoinfoImplL(so);
  } else {
    success = RelinkSoinfoImplM(so);
  }
  if (success) {
    so->set_linked();
  }
  util.RecoveryPageProtect();
  LOGV("The system relink library: %s, result: %s", so->get_soname(), success ? "true" : "false");
  return success;
}

bool ProxyLinker::SystemRelinkLibraries(const std::vector<std::string> &sonames) {
  if (__predict_false(sonames.empty())) {
    return false;
  }
  bool success = true;
  for (size_t i = 0, e = sonames.size(); i < e; ++i) {
    soinfo *so = ProxyLinker::FindSoinfoByName(sonames[i].c_str());
    if (so == nullptr) {
      LOGW("The specified so was not found: %s", sonames[i].c_str());
    } else {
      success &= ProxyLinker::SystemRelinkLibrary(so);
    }
  }
  return success;
}

void *ProxyLinker::CallDlopen(const char *filename, int flags, void *caller_addr, const android_dlextinfo *extinfo) {
  if (android_api >= __ANDROID_API_N__) {
    if (!caller_addr) {
      caller_addr = __builtin_return_address(0);
    }
    return ProxyLinker::Get().CallDoDlopenN(filename, flags, extinfo, caller_addr);
  }
  return ProxyLinker::Get().CallDoDlopen(filename, flags, extinfo);
}

void *ProxyLinker::CallDlsym(void *handle, const char *symbol, void *caller_addr, const char *version) {
  if (android_api >= __ANDROID_API_N__) {
    void *sym = nullptr;
    if (!caller_addr) {
      caller_addr = __builtin_return_address(0);
    }
    ProxyLinker::Get().CallDoDlsymN(handle, symbol, version, caller_addr, &sym);
    return sym;
  }
  return ProxyLinker::Get().CallDoDlsym(handle, symbol);
}

ProxyLinker &ProxyLinker::Get() {
  static ProxyLinker proxy;
  return proxy;
}

} // namespace fakelinker