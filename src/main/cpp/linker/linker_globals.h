//
// Created by beich on 2020/11/5.
//
#pragma once

#include <unordered_map>

#include <linker_export.h>

#include "linker_soinfo.h"

class ProxyLinker {
public:
    static void Init();

#if __ANDROID_API__ >= __ANDROID_API_N__

    static soinfo *SoinfoFromHandle(const void *handle);

    static std::vector<android_namespace_t *> GetAllNamespace(bool flush);

    static android_namespace_t *FindNamespaceByName(const char *name);

    static android_namespace_t *GetDefaultNamespace();

    /*
     * 改变soinfo的命名空间,若np为null则添加到默认命名空间
     * */
    static void ChangeSoinfoOfNamespace(soinfo *so, android_namespace_t *np);

    static bool AddGlobalSoinfoToNamespace(soinfo *global, android_namespace_t *np);

    static void AddSoinfoToDefaultNamespace(soinfo *si);

    static void *CallDoDlopen(const char *filename, int flags, const android_dlextinfo *extinfo, void *caller_addr);

    static bool CallDoDlsym(void *handle, const char *symbol, const char *version, void *caller_addr, void **sym);

    static android_namespace_t *CallCreateNamespace(const char *name, const char *ld_library_path, const char *default_library_path, uint64_t type,
                                                    const char *permitted_when_isolated_path, android_namespace_t *parent_namespace, const void *caller_addr);

#else

    static void *CallDoDlopen(const char *name, int flags, const android_dlextinfo *extinfo);

    static void *CallDoDlsym(void *handle, const char *symbol);

#endif

    /*
     * 当dlopen,dlsym失败时需要设置错误,否者后续调用dlerror而未判断字符串为null时导致错误
     * */
    static void FormatDlerror(const char *msg, const char *detail);

    static void **GetTls();

    static soinfo *FindSoinfoByName(const char *name);

    static soinfo *FindSoinfoByPath(const char *path);

    static soinfo *FindContainingLibrary(const void *p);

    /*
     * linker对应的soinfo结构体,经常使用备份一份
     * */
    static soinfo *GetLinkerSoinfo();

    /*
     * 使用dlsym查找符号,只能查找STB_GLOBAL | STB_WEAK 导出的符号
     * dlsym查找符号如果自己没查找出来还会继续查找依赖so
     *
     * local_group_root
     * 1. Android 7.0以上,自身soinfo排在第一,然后是依赖so
     *
     * dlsym(RTLD_NEXT)行为解析
     * 1. Android 7.0及以上 从调用者所在命名空间的当前 soinfo 的下一个开始查找有 RTLD_GLOBAL 标签的库,
     *    如果没有找到则会继续判断条件然后查找 local_group_root,但是都会排除 caller自身
     *    * Android 9.0及以上则直接从caller local_group_root 开始查找
     *    * Android 7.0 ~ Android 8.1 判断caller库没有 RTLD_GLOBAL 才查找 local_group_root
     * 2. Android 7.0以下 没有命名空间,此时 soinfo是一条链表,从caller soinfo->next开始查找有 RTLD_GLOBAL 标签的库,
     * 	  如果没有找到则判断caller库没有 RTLD_GLOBAL 才查找 local_group_root
     *
     * dlsym(RTLD_DEFAULT)行为解析
     * 1. Android 7.0及以上 从调用者所在命名空间的第一个soinfo开始查找有 RTLD_GLOBAL 标签的库,
     *    如果没找到再同 RTLD_NEXT 一样判断条件从caller local_group_root 开始查找, RTLD_DEFAULT 并不排除caller自身
     *2. Android 7.0以下从第一个 soinfo 开始查找有 RTLD_GLOBAL 标签的库,如果没有找到再继续判断条件从caller local_group_root 开始查找
     * */
    static void *FindSymbolByDlsym(soinfo *si, const char *name);

    static void *GetDlopenOrDlsymAddress(bool dlopen_type);

    static void AddSoinfoToGlobal(soinfo *si);

    static int *GetGLdDebugVerbosity();

    static bool ManualRelinkLibrary(soinfo *global, soinfo *child);

    static bool ManualRelinkLibrary(soinfo *global, soinfo *child, std::vector<std::string> &filters);

    static bool ManualRelinkLibrary(symbol_relocations *rels, soinfo *child);

    static bool ManualRelinkLibraries(soinfo *global, const VarLengthObject<const char *> *vars);

    static bool ManualRelinkLibraries(soinfo *global, const VarLengthObject<const char *> *vars, std::vector<std::string> &filters);

    static bool SystemRelinkLibrary(soinfo *so);

    static bool SystemRelinkLibraries(const VarLengthObject<const char *> *libs);

private:
    static soinfo_list_t GetGlobalGroupOfLowVersion();

    static soinfo_list_t GetSoinfoGlobalGroup(soinfo *root);

    static bool RelinkSoinfoImplOfHighVersion(soinfo *si);

    static bool RelinkSoinfoImplOfLowVersion(soinfo *si);

private:
    static soinfo *solist_ptr;
    static int *g_ld_debug_verbosity_ptr;
    static pthread_mutex_t *g_dl_mutex_ptr;
    static soinfo *linker_soinfo_ptr;
    static char *linker_dl_err_buf_ptr;
    static void *link_image_ptr;
#if  __ANDROID_API__ >= __ANDROID_API_N__
    static std::vector<android_namespace_t *> namespaces;
    static std::unordered_map<uintptr_t, soinfo *> *g_soinfo_handles_map_ptr;
    static android_namespace_t *g_default_namespace_ptr;
#endif
#if  __ANDROID_API__ < __ANDROID_API_M__
    static soinfo **g_ld_preloads_ptr;
#endif
};
