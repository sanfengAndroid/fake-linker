//
// Created by beich on 2020/11/5.
//
#pragma once

#include <unordered_map>

#include <fake_linker.h>
#include <macros.h>

#include "linker_namespaces.h"
#include "linker_soinfo.h"

namespace fakelinker {
class ProxyLinker {
public:
  static ProxyLinker &Get();

  static void *CallDlopen(const char *filename, int flags, void *caller_addr, const android_dlextinfo *extinfo);

  static void *CallDlsym(void *handle, const char *symbol, void *caller_addr, const char *version);

  ANDROID_GE_N soinfo *SoinfoFromHandle(const void *handle);

  ANDROID_GE_N std::vector<android_namespace_t *> GetAllNamespace(bool flush);

  ANDROID_GE_N android_namespace_t *FindNamespaceByName(const char *name);

  ANDROID_GE_N static android_namespace_t *GetDefaultNamespace();

  ANDROID_GE_N static void *GetNamespaceCallerAddress(android_namespace_t *np);
  /*
   * 改变soinfo的命名空间,若np为null则添加到默认命名空间
   * */
  ANDROID_GE_N void ChangeSoinfoOfNamespace(soinfo *so, android_namespace_t *np);

  ANDROID_GE_N bool AddGlobalSoinfoToNamespace(soinfo *global, android_namespace_t *np);

  ANDROID_GE_N bool RemoveSoinfoFromNamespcae(soinfo *so, android_namespace_t *np, bool clear_global_flags);

  ANDROID_GE_N void AddSoinfoToDefaultNamespace(soinfo *si);

  ANDROID_GE_N void *CallDoDlopenN(const char *filename, int flags, const android_dlextinfo *extinfo,
                                   void *caller_addr);

  ANDROID_GE_N bool CallDoDlsymN(void *handle, const char *symbol, const char *version, void *caller_addr, void **sym);

  ANDROID_GE_N android_namespace_t *CallCreateNamespace(const char *name, const char *ld_library_path,
                                                        const char *default_library_path, uint64_t type,
                                                        const char *permitted_when_isolated_path,
                                                        android_namespace_t *parent_namespace, const void *caller_addr);

  ANDROID_LE_M void *CallDoDlopen(const char *name, int flags, const android_dlextinfo *extinfo);

  ANDROID_LE_M void *CallDoDlsym(void *handle, const char *symbol);

  /*
   * 当dlopen,dlsym失败时需要设置错误,否者后续调用dlerror而未判断字符串为null时导致错误
   * */
  void FormatDlerror(const char *msg, const char *detail);

  void **GetTls();

  soinfo *FindSoinfoByName(const char *name);

  soinfo *FindSoinfoByNameInNamespace(const char *name, android_namespace_t *np);

  soinfo *FindSoinfoByPath(const char *path);

  std::vector<soinfo *> GetAllSoinfo();

  soinfo *FindContainingLibrary(const void *p);

  /*
   * linker对应的soinfo结构体,经常使用备份一份
   * */
  soinfo *GetLinkerSoinfo();

  /*
   * 使用dlsym查找符号,只能查找STB_GLOBAL | STB_WEAK 导出的符号
   * dlsym查找符号如果自己没查找出来还会继续查找依赖so
   *
   * local_group_root
   * 1. Android 7.0+ 自身soinfo排在第一,然后是依赖so
   *
   * dlsym(RTLD_NEXT)行为解析
   * 1. Android 7.0+ 从调用者所在命名空间的当前 soinfo 的下一个开始查找有
   * RTLD_GLOBAL 标签的库, 如果没有找到则会继续判断条件然后查找
   * local_group_root,但是都会排除 caller自身
   *    * Android 9.0+ 则直接从caller local_group_root 开始查找
   *    * Android 7.0 ~ Android 8.1 判断caller库没有 RTLD_GLOBAL 才查找
   *
   * local_group_root
   * 2. Android 7.0以下 没有命名空间,此时 soinfo是一条链表,从caller
   * soinfo->next开始查找有 RTLD_GLOBAL 标签的库, 如果没有找到则判断caller库没有
   * RTLD_GLOBAL 才查找 local_group_root
   *
   * dlsym(RTLD_DEFAULT)行为解析
   * 1. Android 7.0及以上 从调用者所在命名空间的第一个soinfo开始查找有
   * RTLD_GLOBAL 标签的库, 如果没找到再同 RTLD_NEXT 一样判断条件从caller
   * local_group_root 开始查找, RTLD_DEFAULT 并不排除caller自身
   * 2. Android 7.0以下从第一个 soinfo 开始查找有 RTLD_GLOBAL
   * 标签的库,如果没有找到再继续判断条件从caller local_group_root 开始查找
   * */
  void *FindSymbolByDlsym(soinfo *si, const char *name);

  void AddSoinfoToGlobal(soinfo *si);

  bool RemoveGlobalSoinfo(soinfo *si);

  bool IsGlobalSoinfo(soinfo *si);

  int *GetGLdDebugVerbosity();
  bool SetLdDebugVerbosity(int level);

  bool ManualRelinkLibrary(soinfo *global, soinfo *child);

  bool ManualRelinkLibrary(soinfo *global, soinfo *child, std::vector<std::string> &filters);

  bool ManualRelinkLibrary(symbol_relocations &rels, soinfo *child);

  bool ManualRelinkLibraries(soinfo *global, const std::vector<std::string> &sonames,
                             const std::vector<std::string> &filters);

  bool ManualRelinkLibraries(soinfo *global, int len, const std::vector<soinfo *> &targets,
                             std::vector<std::string> &filters);

  bool SystemRelinkLibrary(soinfo *so);

  bool SystemRelinkLibraries(const std::vector<std::string> &sonames);

private:
  ANDROID_LE_M soinfo_list_t GetGlobalGroupM();

  soinfo_list_t GetSoinfoGlobalGroup(soinfo *root);

  ANDROID_GE_M bool RelinkSoinfoImplM(soinfo *si);

  ANDROID_LE_L1 bool RelinkSoinfoImplL(soinfo *si);

private:
  ANDROID_GE_N std::vector<android_namespace_t *> namespaces;
};
} // namespace fakelinker
