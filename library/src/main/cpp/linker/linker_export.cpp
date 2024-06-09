//
// Created by beich on 2020/11/15.
//

#include <android/log.h>

#include <fake_linker.h>
#include <linker_version.h>
#include <macros.h>
#include <unique_memory.h>

#include "art/hook_jni_native_interface_impl.h"
#include "elf_reader.h"
#include "linker_globals.h"
#include "linker_soinfo.h"
#include "linker_util.h"
#include "local_block_allocator.h"

using namespace fakelinker;

#define CHECK_PARAM_RET(Param, Err, Ret)                                                                               \
  if (!(Param)) {                                                                                                      \
    if (out_error) {                                                                                                   \
      *out_error = Err;                                                                                                \
    }                                                                                                                  \
    return Ret;                                                                                                        \
  }

#define CHECK_PARAM_BOOL(Param, Err) CHECK_PARAM_RET(Param, Err, false)

#define CHECK_PARAM_PTR(Param, Err)  CHECK_PARAM_RET(Param, Err, nullptr)
#define CHECK_PARAM_INT(Param, Err)  CHECK_PARAM_RET(Param, Err, 0)

#define CHECK_PARAM_RET_ERROR(Param, Err)                                                                              \
  if (!(Param)) {                                                                                                      \
    return Err;                                                                                                        \
  }

#define CHECK_API_PTR(Level)       CHECK_PARAM_PTR(android_api >= (Level), kErrorApiLevel)
#define CHECK_API_INT(Level)       CHECK_PARAM_INT(android_api >= (Level), kErrorApiLevel)
#define CHECK_API_RET_ERROR(Level) CHECK_PARAM_RET_ERROR(android_api >= (Level), kErrorApiLevel)

#define CHECK_ERROR(Cond, Err)                                                                                         \
  if (!(Cond)) {                                                                                                       \
    if (out_error) {                                                                                                   \
      *out_error = Err;                                                                                                \
    }                                                                                                                  \
  }

#define RET_SUCCESS()                                                                                                  \
  if (out_error) {                                                                                                     \
    *out_error = kErrorNo;                                                                                             \
  }

static std::vector<std::string> filter_symbols;

int get_fakelinker_version_impl() { return FAKELINKER_MODULE_VERSION; }

SoinfoPtr soinfo_find_impl(SoinfoFindType find_type, const void *param, int *out_error) {
  RET_SUCCESS();
  switch (find_type) {
  case kSTAddress: {
    if (!param) {
      param = __builtin_return_address(0);
    }
    void *ret = ProxyLinker::Get().FindContainingLibrary(param);
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTHandle: {
    CHECK_API_PTR(__ANDROID_API_N__);
    CHECK_PARAM_PTR(param, kErrorParameterNull);
    void *ret = ProxyLinker::Get().SoinfoFromHandle(param);
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTName: {
    CHECK_PARAM_PTR(param, kErrorParameterNull);
    void *ret = ProxyLinker::Get().FindSoinfoByName(reinterpret_cast<const char *>(param));
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTOrig:
    return const_cast<void *>(param);
  default:
    CHECK_PARAM_PTR(false, kErrorParameter);
    return nullptr;
  }
  return nullptr;
}

ANDROID_GE_N SoinfoPtr soinfo_find_in_namespace_impl(SoinfoFindType find_type, const void *param,
                                                     AndroidNamespacePtr np, int *out_error) {
  CHECK_API_PTR(__ANDROID_API_N__);
  if (np == nullptr) {
    return soinfo_find_impl(find_type, param, out_error);
  }
  switch (find_type) {
  case kSTAddress: {
    if (!param) {
      param = __builtin_return_address(0);
    }
    void *ret = ProxyLinker::Get().FindContainingLibrary(param);
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTHandle: {
    CHECK_API_PTR(__ANDROID_API_N__);
    CHECK_PARAM_PTR(param, kErrorParameterNull);
    void *ret = ProxyLinker::Get().SoinfoFromHandle(param);
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTName: {
    CHECK_PARAM_PTR(param, kErrorParameterNull);
    void *ret = ProxyLinker::Get().FindSoinfoByNameInNamespace(reinterpret_cast<const char *>(param),
                                                               reinterpret_cast<android_namespace_t *>(np));
    CHECK_ERROR(ret, kErrorSoinfoNotFound);
    return ret;
  }
  case kSTOrig:
    return const_cast<void *>(param);
  default:
    CHECK_PARAM_PTR(false, kErrorParameter);
    return nullptr;
  }
  return nullptr;
}

int soinfo_get_attribute_impl(SoinfoPtr soinfo_ptr, SoinfoAttributes *attrs) {
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_RET_ERROR(attrs, kErrorParameterNull);
  auto *so = reinterpret_cast<soinfo *>(soinfo_ptr);
  attrs->soinfo_ptr = so;
  attrs->so_name = so->get_soname();
  attrs->real_path = so->realpath();
  attrs->base = so->base();
  attrs->size = so->size();
  attrs->flags = so->flags();
  if (android_api >= __ANDROID_API_N__) {
    attrs->handle = reinterpret_cast<void *>(so->get_handle());
  } else {
    attrs->handle = so;
  }
  return kErrorNo;
}

void soinfo_to_string_impl(SoinfoPtr soinfo_ptr) {
  if (!soinfo_ptr) {
    return;
  }
  std::string out = soinfo_to_string(reinterpret_cast<soinfo *>(soinfo_ptr));
  LOGI("print soinfo: %s", out.c_str());
}

int soinfo_query_all_impl(MEMORY_FREE SoinfoPtr **out_soinfo_ptr_array, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_INT(out_soinfo_ptr_array, kErrorParameterNull);

  auto vec = ProxyLinker::Get().GetAllSoinfo();
  size_t len = vec.size();
  CHECK_PARAM_INT(len != 0, kErrorNpNull);
  unique_memory memory(sizeof(void *) * len);
  if (!memory.ok()) {
    CHECK_PARAM_INT(false, kErrorMemoryError);
  }
  for (size_t i = 0; i < len; ++i) {
    memory.set(i, vec[i]);
  }
  *out_soinfo_ptr_array = memory.release<SoinfoPtr>();
  return static_cast<int>(len);
}

ANDROID_GE_N SoinfoHandle soinfo_get_handle_impl(SoinfoPtr soinfo_ptr, int *out_error) {
  CHECK_API_PTR(__ANDROID_API_N__);
  RET_SUCCESS();
  CHECK_PARAM_PTR(soinfo_ptr, kErrorSoinfoNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  return reinterpret_cast<void *>(info->get_handle());
}

const char *soinfo_get_name_impl(SoinfoPtr soinfo_ptr, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_PTR(soinfo_ptr, kErrorSoinfoNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  return info->get_soname();
}

bool soinfo_is_global_impl(SoinfoPtr soinfo_ptr, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_BOOL(soinfo_ptr, kErrorSoinfoNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  return ProxyLinker::Get().IsGlobalSoinfo(info);
}

const char *soinfo_get_realpath_impl(SoinfoPtr soinfo_ptr, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_PTR(soinfo_ptr, kErrorSoinfoNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  return info->realpath();
}

SoinfoPtr soinfo_get_linker_soinfo_impl() { return ProxyLinker::Get().GetLinkerSoinfo(); }

SymbolAddress *soinfo_get_import_symbol_address_impl(SoinfoPtr soinfo_ptr, const char *name, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_PTR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_PTR(name, kErrorParameterNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  void *result = info->find_import_symbol_address(name);
  CHECK_ERROR(result, kErrorSymbolNotFoundInSoinfo);
  return reinterpret_cast<SymbolAddress *>(result);
}

SymbolAddress soinfo_get_export_symbol_address_impl(SoinfoPtr soinfo_ptr, const char *name, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_PTR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_PTR(name, kErrorParameterNull);
  auto *info = static_cast<soinfo *>(soinfo_ptr);
  void *result = info->find_export_symbol_address(name);
  CHECK_ERROR(result, kErrorSymbolNotFoundInSoinfo);
  return result;
}

DlopenFun get_dlopen_inside_func_ptr_impl() { return reinterpret_cast<DlopenFun>(ProxyLinker::CallDlopen); }

DlsymFun get_dlsym_inside_func_ptr_impl() {
  return [](void *handle, const char *symbol, void *caller_addr) -> void * {
    if (!caller_addr) {
      caller_addr = __builtin_return_address(0);
    }
    return ProxyLinker::CallDlsym(handle, symbol, caller_addr, nullptr);
  };
}

void *call_dlopen_inside_impl(const char *filename, int flags, void *caller_addr, const AndroidDlextinfoPtr extinfo) {
  if (!caller_addr) {
    caller_addr = __builtin_return_address(0);
  }
  return ProxyLinker::CallDlopen(filename, flags, caller_addr, static_cast<const android_dlextinfo *>(extinfo));
}

void *call_dlsym_inside_impl(void *so, const char *name) {
  if (!so) {
    return nullptr;
  }

  if (android_api >= __ANDROID_API_N__) {
    so = soinfo_find_impl(kSTHandle, so, nullptr);
  }
  if (!so) {
    return nullptr;
  }
  return ProxyLinker::Get().FindSymbolByDlsym(static_cast<soinfo *>(so), name);
}

void *get_linker_export_symbol_impl(const char *name, int *out_error) {
  RET_SUCCESS();
  CHECK_PARAM_PTR(name, kErrorParameterNull);
  auto so = ProxyLinker::Get().GetLinkerSoinfo();
  auto result = so->find_export_symbol_address(name);
  CHECK_ERROR(result, kErrorSymbolNotFoundInSoinfo);
  return result;
}

bool soinfo_add_to_global_impl(SoinfoPtr soinfo_ptr) {
  if (!soinfo_ptr) {
    return false;
  }
  ProxyLinker::Get().AddSoinfoToGlobal(static_cast<soinfo *>(soinfo_ptr));
  return true;
}

bool soinfo_remove_global_impl(SoinfoPtr soinfo_ptr) {
  if (!soinfo_ptr) {
    return false;
  }
  return ProxyLinker::Get().RemoveGlobalSoinfo(static_cast<soinfo *>(soinfo_ptr));
}

void add_relocation_blacklist_impl(const char *symbol_name) {
  if (symbol_name) {
    std::string name = symbol_name;
    if (std::find(filter_symbols.begin(), filter_symbols.end(), name) == filter_symbols.end()) {
      filter_symbols.emplace_back(name);
    }
  }
}

void remove_relocation_blacklist_impl(const char *symbol_name) {
  if (!symbol_name) {
    return;
  }
  auto f = std::find(filter_symbols.begin(), filter_symbols.end(), symbol_name);
  if (f != filter_symbols.end()) {
    filter_symbols.erase(f);
  }
}

void clear_relocation_blacklist_impl() { filter_symbols.clear(); }

bool call_manual_relocation_by_soinfo_impl(SoinfoPtr global_lib, SoinfoPtr target) {
  if (!global_lib || !target) {
    return false;
  }
  return ProxyLinker::Get().ManualRelinkLibrary(static_cast<soinfo *>(global_lib), static_cast<soinfo *>(target),
                                                filter_symbols);
}

bool call_manual_relocation_by_soinfos_impl(SoinfoPtr global_lib, int len, SoinfoPtr targets[]) {
  if (!global_lib || len < 1) {
    return false;
  }
  std::vector<soinfo *> soinfos;
  soinfos.resize(len);
  for (int i = 0; i < len; ++i) {
    if (targets[i] == nullptr) {
      return false;
    }
    soinfos[i] = reinterpret_cast<soinfo *>(targets[i]);
  }
  return ProxyLinker::Get().ManualRelinkLibraries(static_cast<soinfo *>(global_lib), len, soinfos, filter_symbols);
}

bool call_manual_relocation_by_name_impl(SoinfoPtr global_lib, const char *target_name) {
  if (!global_lib || !target_name) {
    return false;
  }
  auto *target = soinfo_find_impl(SoinfoFindType::kSTName, target_name, nullptr);
  if (!target) {
    return false;
  }
  return ProxyLinker::Get().ManualRelinkLibrary(static_cast<soinfo *>(global_lib), static_cast<soinfo *>(target),
                                                filter_symbols);
}

bool call_manual_relocation_by_names_impl(SoinfoPtr global_lib, int len, const char *target_names[]) {
  if (!global_lib || len < 1) {
    return false;
  }

  std::vector<std::string> sonames;
  sonames.resize(len);
  for (int i = 0; i < len; ++i) {
    if (target_names[i] == nullptr) {
      return false;
    }
    sonames[i] = target_names[i];
  }
  return ProxyLinker::Get().ManualRelinkLibraries(static_cast<soinfo *>(global_lib), sonames, filter_symbols);
}

ANDROID_GE_N AndroidNamespacePtr android_namespace_find_impl(NamespaceFindType find_type, const void *param,
                                                             int *out_error) {
  CHECK_API_PTR(__ANDROID_API_N__);
  android_namespace_t *result = nullptr;
  soinfo *so = nullptr;
  RET_SUCCESS();
  CHECK_PARAM_PTR(param, kErrorParameterNull);

  switch (find_type) {
  case kNPOriginal:
    result = reinterpret_cast<android_namespace_t *>(const_cast<void *>(param));
    break;
  case kNPSoinfo:
    so = const_cast<soinfo *>(reinterpret_cast<const soinfo *>(param));
    break;
  case kNPNamespaceName:
    result = ProxyLinker::Get().FindNamespaceByName(reinterpret_cast<const char *>(param));
    break;
  default:
    CHECK_ERROR(false, kErrorParameter);
    break;
  }
  if (result == nullptr && so != nullptr) {
    result = so->get_primary_namespace();
  }
  CHECK_ERROR(result, kErrorNpNotFound);
  return result;
}

ANDROID_GE_N const char *android_namespace_get_name_impl(AndroidNamespacePtr android_namespace_ptr, int *out_error) {
  CHECK_API_PTR(__ANDROID_API_N__);
  RET_SUCCESS();
  CHECK_PARAM_PTR(android_namespace_ptr, kErrorNpNull);
  android_namespace_t *anp = reinterpret_cast<android_namespace_t *>(android_namespace_ptr);
  return anp->get_name();
}

ANDROID_GE_N int android_namespace_query_all_impl(MEMORY_FREE AndroidNamespacePtr **out_android_namespace_ptr_array,
                                                  int *out_error) {
  CHECK_API_INT(__ANDROID_API_N__);
  RET_SUCCESS();
  CHECK_PARAM_INT(out_android_namespace_ptr_array, kErrorParameterNull);
  auto vec = ProxyLinker::Get().GetAllNamespace(true);
  size_t len = vec.size();
  CHECK_PARAM_INT(len != 0, kErrorNpNull);
  CHECK_PARAM_INT(out_android_namespace_ptr_array, kErrorParameterNull);

  unique_memory memory(sizeof(void *) * len);
  if (!memory.ok()) {
    return kErrorMemoryError;
  }
  for (size_t i = 0; i < len; ++i) {
    memory.set(i, vec[i]);
  }
  *out_android_namespace_ptr_array = memory.release<AndroidNamespacePtr>();
  return static_cast<int>(len);
}

AndroidNamespacePtr android_namespace_create_impl(const char *name, const char *ld_library_path,
                                                  const char *default_library_path, uint64_t type,
                                                  const char *permitted_when_isolated_path,
                                                  AndroidNamespacePtr parent_namespace, const void *caller_addr) {
  return ProxyLinker::Get().CallCreateNamespace(name, ld_library_path, default_library_path, type,
                                                permitted_when_isolated_path,
                                                static_cast<android_namespace_t *>(parent_namespace), caller_addr);
}

ANDROID_GE_N int android_namespace_get_all_soinfo_impl(AndroidNamespacePtr android_namespace_ptr,
                                                       MEMORY_FREE SoinfoPtr **out_soinfo_ptr_array, int *out_error) {
  CHECK_API_INT(__ANDROID_API_N__);
  RET_SUCCESS();
  auto *an = static_cast<android_namespace_t *>(android_namespace_ptr);
  CHECK_PARAM_INT(an, kErrorNpNull);
  CHECK_PARAM_INT(out_soinfo_ptr_array, kErrorParameterNull);
  size_t size = an->soinfo_list().size();

  unique_memory memory(sizeof(void *) * size);
  if (!memory.ok()) {
    return kErrorMemoryError;
  }
  int index = 0;
  an->soinfo_list().for_each([&](soinfo *_so) {
    memory.set(index++, _so);
  });
  *out_soinfo_ptr_array = memory.release<SoinfoPtr>();
  return static_cast<int>(size);
}

ANDROID_GE_N void *android_namespace_get_caller_address_impl(AndroidNamespacePtr android_namespace_ptr,
                                                             int *out_error) {
  CHECK_API_PTR(__ANDROID_API_N__);
  RET_SUCCESS();

  auto *np = static_cast<android_namespace_t *>(android_namespace_ptr);
  CHECK_PARAM_PTR(np, kErrorNpNull);
  return ProxyLinker::GetNamespaceCallerAddress(np);
}

ANDROID_GE_O int android_namespace_get_link_namespace_impl(AndroidNamespacePtr android_namespace_ptr,
                                                           ONLY_READ AndroidLinkNamespacePtr *link_np_array,
                                                           int *out_error) {
  CHECK_API_INT(__ANDROID_API_O__);
  RET_SUCCESS();
  CHECK_PARAM_INT(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_INT(link_np_array, kErrorParameterNull);
  auto *np = static_cast<android_namespace_t *>(android_namespace_ptr);
  if (android_api >= __ANDROID_API_P__) {
    auto &vec = np->linked_namespaceP();
    *link_np_array = (void **)&vec[0];
    return static_cast<int>(vec.size());
  }
  auto &vec = np->linked_namespacesO();
  *link_np_array = (void **)&vec[0];
  return static_cast<int>(vec.size());
}

ANDROID_GE_N int android_namespace_get_global_soinfos_impl(AndroidNamespacePtr android_namespace_ptr,
                                                           MEMORY_FREE SoinfoPtr **out_soinfo_ptr_array,
                                                           int *out_error) {
  CHECK_API_INT(__ANDROID_API_N__);
  RET_SUCCESS();
  CHECK_PARAM_INT(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_INT(out_soinfo_ptr_array, kErrorParameterNull);
  soinfo_list_t list = static_cast<android_namespace_t *>(android_namespace_ptr)->get_global_group();
  size_t size = list.size();

  unique_memory memory(sizeof(SoinfoPtr) * size);
  if (!memory.ok()) {
    return kErrorMemoryError;
  }
  CHECK_PARAM_INT(out_soinfo_ptr_array, kErrorMemoryError);
  int index = 0;
  list.for_each([&](soinfo *_so) {
    memory.set(index++, _so);
  });
  *out_soinfo_ptr_array = memory.release<SoinfoPtr>();
  return static_cast<int>(size);
}

ANDROID_GE_N int android_namespace_add_global_soinfo_impl(AndroidNamespacePtr android_namespace_ptr,
                                                          SoinfoPtr global_soinfo_ptr) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(global_soinfo_ptr, kErrorSoinfoNull);

  bool ret = ProxyLinker::Get().AddGlobalSoinfoToNamespace(static_cast<soinfo *>(global_soinfo_ptr),
                                                           static_cast<android_namespace_t *>(android_namespace_ptr));

  return ret ? kErrorNo : kErrorExec;
}

ANDROID_GE_N int android_namespace_add_soinfo_impl(AndroidNamespacePtr android_namespace_ptr, SoinfoPtr soinfo_ptr) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  static_cast<android_namespace_t *>(android_namespace_ptr)->add_soinfo(static_cast<const soinfo *>(soinfo_ptr));
  return kErrorNo;
}

ANDROID_GE_N int android_namespace_remove_soinfo_impl(AndroidNamespacePtr android_namespace_ptr, SoinfoPtr soinfo_ptr,
                                                      bool clear_global_flags) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  if (ProxyLinker::Get().RemoveSoinfoFromNamespcae(static_cast<soinfo *>(soinfo_ptr),
                                                   static_cast<android_namespace_t *>(android_namespace_ptr),
                                                   clear_global_flags)) {
    return FakeLinkerError::kErrorNo;
  }
  return FakeLinkerError::kErrorExec;
}

ANDROID_GE_Q int android_namespace_get_white_list_impl(AndroidNamespacePtr android_namespace_ptr,
                                                       MEMORY_FREE const char **out_white_list, int *out_error) {
  CHECK_API_INT(__ANDROID_API_Q__);
  CHECK_PARAM_INT(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_INT(out_white_list, kErrorParameterNull);
  RET_SUCCESS();
  android_namespace_t *np = static_cast<android_namespace_t *>(android_namespace_ptr);
  auto &libs = np->get_whitelisted_libs();
  if (libs.empty()) {
    *out_white_list = nullptr;
    return 0;
  }
  unique_memory memory(sizeof(const char *) * libs.size());
  if (!memory.ok()) {
    return kErrorMemoryError;
  }
  size_t index = 0;
  for (auto &name : libs) {
    memory.set(index++, name.c_str());
  }
  *out_white_list = memory.release<const char>();
  return libs.size();
}

ANDROID_GE_Q int android_namespace_add_soinfo_whitelist_impl(AndroidNamespacePtr android_namespace_ptr,
                                                             const char *libname) {
  CHECK_API_RET_ERROR(__ANDROID_API_Q__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(libname, kErrorParameterNull);
  static_cast<android_namespace_t *>(android_namespace_ptr)->add_whitelisted_lib(libname);
  return kErrorNo;
}

ANDROID_GE_Q int android_namespace_remove_whitelist_impl(AndroidNamespacePtr android_namespace_ptr,
                                                         const char *libname) {
  CHECK_API_RET_ERROR(__ANDROID_API_Q__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(libname, kErrorParameterNull);
  android_namespace_t *np = static_cast<android_namespace_t *>(android_namespace_ptr);
  np->remove_whitelisted_lib(libname);
  return kErrorNo;
}

ANDROID_GE_N int android_namespace_add_ld_library_path_impl(AndroidNamespacePtr android_namespace_ptr,
                                                            const char *path) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(path, kErrorParameterNull);
  static_cast<android_namespace_t *>(android_namespace_ptr)->add_ld_library_path(path);
  return kErrorNo;
}

ANDROID_GE_N int android_namespace_add_default_library_path_impl(AndroidNamespacePtr android_namespace_ptr,
                                                                 const char *path) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(path, kErrorParameterNull);
  return kErrorUnavailable;
  static_cast<android_namespace_t *>(android_namespace_ptr)->add_default_library_path(path);
  return kErrorNo;
}

ANDROID_GE_N int android_namespace_add_permitted_library_path_impl(AndroidNamespacePtr android_namespace_ptr,
                                                                   const char *path) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(path, kErrorParameterNull);
  return kErrorUnavailable;
  static_cast<android_namespace_t *>(android_namespace_ptr)->add_permitted_path(path);
  return kErrorNo;
}

ANDROID_GE_O int android_namespace_add_linked_namespace_impl(AndroidNamespacePtr android_namespace_ptr,
                                                             AndroidNamespacePtr add_namespace_ptr,
                                                             ANDROID_GE_P bool allow_all_shared_libs, int len,
                                                             const char *shared_libs[]) {
  CHECK_API_RET_ERROR(__ANDROID_API_O__);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  CHECK_PARAM_RET_ERROR(add_namespace_ptr, kErrorParameterNull);
  CHECK_PARAM_RET_ERROR(len > 0, kErrorParameter);

  auto *np = static_cast<android_namespace_t *>(android_namespace_ptr);
  std::unordered_set<std::string> shared;
  for (int i = 0; i < len; ++i) {
    shared.insert(shared_libs[i]);
  }
  if (android_api >= __ANDROID_API_P__) {
    android_namespace_link_t_P link(static_cast<android_namespace_t *>(add_namespace_ptr), shared,
                                    allow_all_shared_libs);
    np->add_linked_namespace(&link);
  } else {
    android_namespace_link_t_O link(static_cast<android_namespace_t *>(add_namespace_ptr), shared);
    np->add_linked_namespace(&link);
  }
  return kErrorNo;
}

ANDROID_GE_N int soinfo_add_second_namespace_impl(SoinfoPtr soinfo_ptr, AndroidNamespacePtr android_namespace_ptr) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  static_cast<soinfo *>(soinfo_ptr)->add_secondary_namespace(static_cast<android_namespace_t *>(android_namespace_ptr));
  return kErrorNo;
}

ANDROID_GE_N int soinfo_remove_second_namespace_impl(SoinfoPtr soinfo_ptr, AndroidNamespacePtr android_namespace_ptr) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  static_cast<soinfo *>(soinfo_ptr)
    ->remove_secondary_namespace(static_cast<android_namespace_t *>(android_namespace_ptr));
  return kErrorNo;
}

ANDROID_GE_N int soinfo_change_namespace_impl(SoinfoPtr soinfo_ptr, AndroidNamespacePtr android_namespace_ptr) {
  CHECK_API_RET_ERROR(__ANDROID_API_N__);
  CHECK_PARAM_RET_ERROR(soinfo_ptr, kErrorSoinfoNull);
  CHECK_PARAM_RET_ERROR(android_namespace_ptr, kErrorNpNull);
  ProxyLinker::Get().ChangeSoinfoOfNamespace(static_cast<soinfo *>(soinfo_ptr),
                                             static_cast<android_namespace_t *>(android_namespace_ptr));
  return kErrorNo;
}

int android_log_print_impl(int prio, const char *tag, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = __android_log_vprint(prio, tag, fmt, va);
  va_end(va);
  return ret;
}

uint64_t find_library_symbol_impl(const char *library_name, const char *symbol_name, const FindSymbolType symbol_type) {
  if (!library_name || !symbol_name) {
    return 0;
  }
  ElfReader reader;
  switch (symbol_type) {
  case FindSymbolType::kImported:
    reader.LoadFromMemory(library_name);
    return reader.FindImportSymbol(symbol_name);
  case FindSymbolType::kExported:
    reader.LoadFromMemory(library_name);
    return reader.FindExportSymbol(symbol_name);
  case FindSymbolType::kInternal:
    reader.LoadFromDisk(library_name);
    return reader.FindInternalSymbol(symbol_name);
  default:
    break;
  }
  return 0;
}

MEMORY_FREE uint64_t *find_library_symbols_impl(const char *library_name, const FindSymbolUnit *symbols, int size) {
  if (!library_name || !symbols || size < 1) {
    return nullptr;
  }

  std::vector<std::string> imports;
  std::vector<std::string> exports;
  std::vector<std::string> internals;

  for (int i = 0; i < size; ++i) {
    switch (symbols[i].symbol_type) {
    case FindSymbolType::kImported:
      imports.push_back(symbols[i].symbol_name);
      break;
    case FindSymbolType::kExported:
      exports.push_back(symbols[i].symbol_name);
      break;
    case FindSymbolType::kInternal:
      internals.push_back(symbols[i].symbol_name);
      break;
    default:
      break;
    }
  }

  ElfReader reader;
  if (!imports.empty() || exports.empty()) {
    reader.LoadFromMemory(library_name);
  }
  if (!internals.empty()) {
    reader.LoadFromDisk(library_name);
  }
  std::vector<Address> import_addrs = reader.FindImportSymbols(imports);
  std::vector<Address> export_addrs = reader.FindExportSymbols(exports);
  std::vector<Address> internal_addrs = reader.FindImportSymbols(internals);

  std::vector<Address> result;

  for (int i = size - 1; i >= 0; ++i) {
    switch (symbols[i].symbol_type) {
    case FindSymbolType::kImported:
      result.insert(result.begin(), import_addrs.back());
      import_addrs.pop_back();
      break;
    case FindSymbolType::kExported:
      result.insert(result.begin(), export_addrs.back());
      export_addrs.pop_back();
      break;
    case FindSymbolType::kInternal:
      result.insert(result.begin(), internal_addrs.back());
      internal_addrs.pop_back();
    default:
      break;
    }
  }
  unique_memory memory(sizeof(Address) * result.size());
  for (int i = 0; i < size; ++i) {
    memory.set(i, result[i]);
  }
  return memory.release<uint64_t>();
}

bool set_ld_debug_verbosity_impl(int level) { return ProxyLinker::Get().SetLdDebugVerbosity(level); }

C_API API_PUBLIC FakeLinker g_fakelinker_export = {
  get_fakelinker_version_impl,
  []() {
    return init_success;
  },
  soinfo_find_impl,
  soinfo_find_in_namespace_impl,
  soinfo_get_attribute_impl,
  soinfo_to_string_impl,
  soinfo_query_all_impl,
  soinfo_get_handle_impl,
  soinfo_get_name_impl,
  soinfo_is_global_impl,
  soinfo_get_realpath_impl,
  soinfo_get_linker_soinfo_impl,
  soinfo_get_import_symbol_address_impl,
  soinfo_get_export_symbol_address_impl,
  []() -> SoinfoPtr {
    return ProxyLinker::Get().FindContainingLibrary(reinterpret_cast<void *>(ProxyLinker::Get));
  },
  get_dlopen_inside_func_ptr_impl,
  get_dlsym_inside_func_ptr_impl,
  call_dlopen_inside_impl,
  call_dlsym_inside_impl,
  get_linker_export_symbol_impl,
  soinfo_add_to_global_impl,
  soinfo_remove_global_impl,
  add_relocation_blacklist_impl,
  remove_relocation_blacklist_impl,
  clear_relocation_blacklist_impl,
  call_manual_relocation_by_soinfo_impl,
  call_manual_relocation_by_soinfos_impl,
  call_manual_relocation_by_name_impl,
  call_manual_relocation_by_names_impl,
  nullptr, /* unused0 */
  nullptr, /* unused1 */
  nullptr, /* unused2 */
  nullptr, /* unused3 */
  nullptr, /* unused4 */
  nullptr, /* unused5 */
  nullptr, /* unused6 */
  nullptr, /* unused7 */
  nullptr, /* unused8 */
  nullptr, /* unused9 */
  android_namespace_find_impl,
  android_namespace_get_name_impl,
  android_namespace_query_all_impl,
  android_namespace_create_impl,
  android_namespace_get_all_soinfo_impl,
  android_namespace_get_caller_address_impl,
  android_namespace_get_link_namespace_impl,
  android_namespace_get_global_soinfos_impl,
  android_namespace_add_global_soinfo_impl,
  android_namespace_add_soinfo_impl,
  android_namespace_remove_soinfo_impl,
  android_namespace_get_white_list_impl,
  android_namespace_add_soinfo_whitelist_impl,
  android_namespace_remove_whitelist_impl,
  android_namespace_add_ld_library_path_impl,
  android_namespace_add_default_library_path_impl,
  android_namespace_add_permitted_library_path_impl,
  android_namespace_add_linked_namespace_impl,
  soinfo_add_second_namespace_impl,
  soinfo_remove_second_namespace_impl,
  soinfo_change_namespace_impl,
  nullptr, /* unused10 */
  nullptr, /* unused11 */
  nullptr, /* unused12 */
  nullptr, /* unused13 */
  nullptr, /* unused14 */
  nullptr, /* unused15 */
  nullptr, /* unused16 */
  nullptr, /* unused17 */
  nullptr, /* unused18 */
  nullptr, /* unused19 */
  [](const void *ptr) {
    unique_memory memory(const_cast<void *>(ptr));
  },
  HookJniNativeInterface,
  HookJniNativeInterfaces,
  HookJavaNativeFunctions,
  nullptr, /* unused20 */
  nullptr, /* unused21 */
  nullptr, /* unused22 */
  nullptr, /* unused23 */
  nullptr, /* unused24 */
  nullptr, /* unused25 */
  nullptr, /* unused26 */
  nullptr, /* unused27 */
  nullptr, /* unused28 */
  nullptr, /* unused29 */
  android_log_print_impl,
  find_library_symbol_impl,
  find_library_symbols_impl,
  set_ld_debug_verbosity_impl,
};

C_API FakeLinker *get_fakelinker() { return &g_fakelinker_export; }