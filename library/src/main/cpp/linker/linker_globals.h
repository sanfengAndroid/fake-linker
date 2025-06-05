//
// Created by beich on 2020/11/5.
//
#pragma once

#include <fakelinker/fake_linker.h>
#include <fakelinker/macros.h>

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
   * Change soinfo's namespace, if np is null then add to default namespace
   */
  ANDROID_GE_N void ChangeSoinfoOfNamespace(soinfo *so, android_namespace_t *np);

  ANDROID_GE_N bool AddGlobalSoinfoToNamespace(soinfo *global, android_namespace_t *np);

  ANDROID_GE_N bool RemoveSoinfoFromNamespace(soinfo *so, android_namespace_t *np, bool clear_global_flags);

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
   * When dlopen or dlsym fails, error needs to be set, otherwise subsequent calls to dlerror
   * without checking if the string is null will cause errors
   */
  void FormatDlerror(const char *msg, const char *detail);

  void **GetTls();

  soinfo *FindSoinfoByName(const char *name);

  soinfo *FindSoinfoByNameInNamespace(const char *name, android_namespace_t *np);

  soinfo *FindSoinfoByPath(const char *path);

  std::vector<soinfo *> GetAllSoinfo();

  soinfo *FindContainingLibrary(const void *p);

  /*
   * Linker's corresponding soinfo structure, frequently used so keep a backup copy
   */
  soinfo *GetLinkerSoinfo();

  /*
   * Use dlsym to find symbols, can only find STB_GLOBAL | STB_WEAK exported symbols
   * When dlsym searches for symbols, if it doesn't find them in itself, it will continue to search in dependent SOs
   *
   * local_group_root
   * 1. Android 7.0+ the soinfo itself is placed first, then dependent SOs
   *
   * dlsym(RTLD_NEXT) behavior analysis
   * 1. Android 7.0+ starts searching from the next soinfo after the current soinfo in the caller's namespace for
   * libraries with RTLD_GLOBAL flag. If not found, it will continue to check conditions and then search
   * local_group_root, but will exclude the caller itself
   *    * Android 9.0+ directly starts searching from caller local_group_root
   *    * Android 7.0 ~ Android 8.1 only searches if the caller library doesn't have RTLD_GLOBAL
   *
   * local_group_root
   * 2. Android 7.0 and below have no namespace, at this time soinfo is a linked list, starting from caller
   * soinfo->next to search for libraries with RTLD_GLOBAL flag. If not found, then check if caller library doesn't have
   * RTLD_GLOBAL flag to search local_group_root
   *
   * dlsym(RTLD_DEFAULT) behavior analysis
   * 1. Android 7.0 and above start searching from the first soinfo in the caller's namespace for
   * libraries with RTLD_GLOBAL flag. If not found, then same as RTLD_NEXT, check conditions from caller
   * local_group_root to start searching, RTLD_DEFAULT does not exclude caller itself
   * 2. Android 7.0 and below start searching from the first soinfo for libraries with RTLD_GLOBAL
   * flag. If not found, then continue to check conditions and start searching from caller local_group_root
   */
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

  soinfo_list_t GetSoinfoGlobalGroup(soinfo *root);

private:
  ANDROID_LE_M soinfo_list_t GetGlobalGroupM();

  ANDROID_GE_M bool RelinkSoinfoImplM(soinfo *si);

  ANDROID_LE_L1 bool RelinkSoinfoImplL(soinfo *si);

private:
  ANDROID_GE_N std::vector<android_namespace_t *> namespaces;
};
} // namespace fakelinker
