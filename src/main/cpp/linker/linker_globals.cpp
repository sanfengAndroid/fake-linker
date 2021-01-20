#include <elf.h>
//
// Created by beich on 2020/11/5.
//

/*
 * Android 5.0 x86 soinfo_link_image函数被优化不遵循cdecl调用约定，因此无法使用系统重定位
 * Android 5.1 x86_64 LinkImage 被内联了
 * */

#include "linker_globals.h"

#include <alog.h>
#include <maps_util.h>
#include <macros.h>

#include "linker_namespaces.h"
#include "fake_linker.h"
#include "linker_soinfo.h"
#include "linker_util.h"
#include "scoped_pthread_mutex_locker.h"
#include "bionic/get_tls.h"

#include <algorithm>
#include <dlfcn.h>
#include <sys/mman.h>

#define DEFAULT_NAMESPACE_NAME "(default)"

#if __ANDROID_API__ >= __ANDROID_API_N__

static void *(*dlopen_ptr)(const char *filename, int flags, const android_dlextinfo *extinfo, const void *caller_addr);

    #if __ANDROID_API__ >= __ANDROID_API_O__

/*
    * Linker导出的符号 __loader_dlopen, __loader_dlsym,已经包含返回地址可以直接使用
    * */
    static void *(*dlsym_ptr)(void *handle, const char *symbol, const void *caller_addr);

    // 已经包含caller地址则无需查找内部符号
    static android_namespace_t *(*create_namespace_ptr)(const char *name, const char *ld_library_path, const char *default_library_path, uint64_t type,
                                                        const char *permitted_when_isolated_path, android_namespace_t *parent_namespace, const void *caller_addr);

    #else

/*
 * 在Android7中Linker导出 dlopen, dlsym符号不包含caller地址,且不同设备可能内联了dlopen_ext和dlsym_impl符号,因此查找更底层内部符号do_dlopen,do_dlsym
 * 且还要修复dlerror
 * */
static bool (*dlsym_ptr)(void *handle, const char *symbol, const char *version, void *caller_addr, void **sym);

static android_namespace_t *(*create_namespace_ptr)(const void *caller_addr, const char *name, const char *ld_library_path, const char *default_library_path, uint64_t type,
                                                    const char *permitted_when_isolated_path, android_namespace_t *parent_namespace);

    #endif
#else

static void *(*dlopen_ptr)(const char *filename, int flags, const android_dlextinfo *extinfo);

    static void *(*dlsym_ptr)(void *handle, const char *symbol);

#endif

#if defined(__LP64__)
    #define LP_TAIL "Pm"
#else
    #define LP_TAIL "Pj"
#endif
/*
 * 关于dlopen dlsym
 * Android 8.0 以上 linker导出符号 __loader_dlopen, 0__loader_dlsym 可以直接修改caller地址
 * Android 7.0 ~ Android 7.1.2 linker 中导出符号 dlopen, dlsym ,其可能内联了dlopen_ext,dlsym_impl函数因此直接调用无法修改
 *  而内联了则要查找do_dlopen,do_dlsym函数,而当失败时还要修复dlerror,且__bionic_format_dlerror也被内联了
 * Android 7.0以下直接使用原生dlopen和dlsym即可
 * */
#if __ANDROID_API__ >= __ANDROID_API_N__

    #if __ANDROID_API__ >= __ANDROID_API_O__
        #define DO_DLOPEN_NAME "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv"
        #define DO_DLSYM_NAME "__dl__Z8do_dlsymPvPKcS1_PKvPS_"
    #else
        #define DO_DLOPEN_NAME "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"
        #define DO_DLSYM_NAME "__dl__Z8do_dlsymPvPKcS1_S_PS_"
    #endif

    #if __ANDROID_API__ <= __ANDROID_API_N_MR1__
        #define  SOINFO_HANDLES_NAME "__dl__ZL20g_soinfo_handles_map"
    #else
        #define  SOINFO_HANDLES_NAME "__dl_g_soinfo_handles_map"
    #endif

#else
// Android 拦截java调用android_dlopen_ext使用
    #define DO_DLOPEN_NAME "__dl__Z9do_dlopenPKciPK17android_dlextinfo"
#endif

#if __ANDROID_API__ >= __ANDROID_API_R__
    #define LINK_IMAGE_NAME "__dl__ZN6soinfo10link_imageERK16SymbolLookupListPS_PK17android_dlextinfo" LP_TAIL
#elif __ANDROID_API__ == __ANDROID_API_Q__
    #define LINK_IMAGE_NAME "__dl__ZN6soinfo10link_imageERK10LinkedListIS_19SoinfoListAllocatorES4_PK17android_dlextinfo" LP_TAIL
#elif __ANDROID_API__ >= __ANDROID_API_M__
    #define LINK_IMAGE_NAME "__dl__ZN6soinfo10link_imageERK10LinkedListIS_19SoinfoListAllocatorES4_PK17android_dlextinfo"
#elif __ANDROID_API__ == __ANDROID_API_L_MR1__
// Android 5.1 x86_64 该方法被内联了,需要单独适配
    #define LINK_IMAGE_NAME "__dl__ZN6soinfo9LinkImageEPK17android_dlextinfo"
#else
    #define LINK_IMAGE_NAME "__dl__ZL17soinfo_link_imageP6soinfoPK17android_dlextinfo"
#endif

/*
 * Android 11
 * lookup_list包含所有可以访问的全局组和本地组, local_group_root this
 * bool soinfo::link_image(const SymbolLookupList& lookup_list, soinfo* local_group_root,
                        const android_dlextinfo* extinfo, size_t* relro_fd_offset)
 * Android 10
 * bool soinfo::link_image(const soinfo_list_t& global_group, const soinfo_list_t& local_group,
                  const android_dlextinfo* extinfo, size_t* relro_fd_offset)
 * Android 9, 8.1, 8,7.1,7,6.0
 *  bool soinfo::link_image(const soinfo_list_t& global_group, const soinfo_list_t& local_group,
                  const android_dlextinfo* extinfo)
 * Android 5.1
 * bool soinfo::LinkImage(const android_dlextinfo* extinfo);
 * Android 5.0 static bool soinfo_link_image(soinfo* si, const android_dlextinfo* extinfo)
 * */
void *ProxyLinker::link_image_ptr = nullptr;
soinfo *ProxyLinker::solist_ptr = nullptr;
int *ProxyLinker::g_ld_debug_verbosity_ptr = nullptr;
soinfo *ProxyLinker::linker_soinfo_ptr = nullptr;
char *ProxyLinker::linker_dl_err_buf_ptr = nullptr;
/*
 * Linker的一些操作需要加锁
 * */
pthread_mutex_t *ProxyLinker::g_dl_mutex_ptr = nullptr;

#if __ANDROID_API__ >= __ANDROID_API_N__
android_namespace_t *ProxyLinker::g_default_namespace_ptr = nullptr;
std::vector<android_namespace_t *> ProxyLinker::namespaces;
std::unordered_map<uintptr_t, soinfo *> *ProxyLinker::g_soinfo_handles_map_ptr = nullptr;
#endif
#if __ANDROID_API__ < __ANDROID_API_M__
soinfo **ProxyLinker::g_ld_preloads_ptr = nullptr;
#endif

typedef struct {
    const char *name;
    int index;
    gaddress old_address;
    gaddress new_address;
} symbol_observed;

enum walk_action_result_t : uint32_t {
    kWalkStop = 0,
    kWalkContinue = 1,
    kWalkSkip = 2
};

#if __ANDROID_API__ >= __ANDROID_API_N__

soinfo *ProxyLinker::SoinfoFromHandle(const void *handle) {

    if ((reinterpret_cast<uintptr_t>(handle) & 1) != 0) {
        auto it = g_soinfo_handles_map_ptr->find(reinterpret_cast<uintptr_t>(handle));
        if (it == g_soinfo_handles_map_ptr->end()) {
            return nullptr;
        } else {
            return it->second;
        }
    }
    return static_cast<soinfo *>(const_cast<void *>(handle));
}

std::vector<android_namespace_t *> ProxyLinker::GetAllNamespace(bool flush) {

    if (!namespaces.empty() && !flush) {
        return namespaces;
    }
    soinfo *si = solist_ptr;
    do {
        auto find = std::find_if(namespaces.begin(), namespaces.end(), [&](android_namespace_t *t) {
            return t == si->primary_namespace_;
        });
        if (find == namespaces.end()) {
            namespaces.push_back(si->primary_namespace_);
        }
    } while ((si = si->next) != nullptr);
    return namespaces;
}

android_namespace_t *ProxyLinker::FindNamespaceByName(const char *name) {
    if (name == nullptr) {
        return nullptr;
    }
    for (const auto &np :ProxyLinker::GetAllNamespace(true)) {
        if (np->get_name() != nullptr && strcmp(np->get_name(), name) == 0) {
            return np;
        }
    }
    return nullptr;
}

android_namespace_t *ProxyLinker::GetDefaultNamespace() {
    return g_default_namespace_ptr;
}

bool ProxyLinker::AddGlobalSoinfoToNamespace(soinfo *global, android_namespace_t *np) {
    if (__predict_false(global == nullptr) || __predict_false(np == nullptr)) {
        return false;
    }

    linker_block_protect_all(PROT_READ | PROT_WRITE);
    np->add_soinfo(global);
    global->add_secondary_namespace(np);
    linker_block_protect_all(PROT_READ);
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
    linker_block_protect_all(PROT_READ | PROT_WRITE);

    so->primary_namespace_->remove_soinfo(so);
    so->primary_namespace_ = np;
    np->add_soinfo(so);
}

void ProxyLinker::AddSoinfoToDefaultNamespace(soinfo *si) {
    if (si == nullptr) {
        return;
    }

    if (strcmp(si->get_primary_namespace()->get_name(), DEFAULT_NAMESPACE_NAME) == 0) {
        return;
    }
    linker_block_protect_all(PROT_READ | PROT_WRITE);
    for (auto &np : ProxyLinker::GetAllNamespace(true)) {
        if (strcmp(np->get_name(), DEFAULT_NAMESPACE_NAME) == 0) {
            si->primary_namespace_->remove_soinfo(si);
            si->primary_namespace_ = np;
            np->add_soinfo(si);
            break;
        }
    }
}

void *ProxyLinker::CallDoDlopen(const char *filename, int flags, const android_dlextinfo *extinfo, void *caller_addr) {
#if __ANDROID_API__ >= __ANDROID_API_O__
    return dlopen_ptr(filename, flags, extinfo, caller_addr);
#else
    ScopedPthreadMutexLocker locker(g_dl_mutex_ptr);
    void *result = dlopen_ptr(filename, flags, extinfo, caller_addr);
    if (result == nullptr) {
        FormatDlerror("dlopen failed", linker_dl_err_buf_ptr);
    }
    return result;
#endif
}

bool ProxyLinker::CallDoDlsym(void *handle, const char *symbol, const char *version, void *caller_addr, void **sym) {
#if __ANDROID_API__ >= __ANDROID_API_O__
    *sym = dlsym_ptr(handle, symbol, caller_addr);
    return *sym != nullptr;
#else
    // 符号没找到要设置错误输出,避免后续调用dlerror出错
    ScopedPthreadMutexLocker locker(g_dl_mutex_ptr);
    bool found = dlsym_ptr(handle, symbol, version, caller_addr, sym);
    if (!found) {
        FormatDlerror(linker_dl_err_buf_ptr, nullptr);
    }
    return found;
#endif
}

android_namespace_t *ProxyLinker::CallCreateNamespace(const char *name, const char *ld_library_path, const char *default_library_path, uint64_t type,
                                                      const char *permitted_when_isolated_path, android_namespace_t *parent_namespace, const void *caller_addr) {
#if __ANDROID_API__ >= __ANDROID_API_O__
    return create_namespace_ptr(name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_namespace, caller_addr);
#else
    ScopedPthreadMutexLocker locker(g_dl_mutex_ptr);
    android_namespace_t *result = create_namespace_ptr(caller_addr, name, ld_library_path, default_library_path, type, permitted_when_isolated_path, parent_namespace);
    if (result == nullptr) {
        FormatDlerror("android_create_namespace failed", linker_dl_err_buf_ptr);
    }
    return result;
#endif
}

#else

void *ProxyLinker::CallDoDlopen(const char *name, int flags, const android_dlextinfo *extinfo) {
    return dlopen_ptr(name, flags, extinfo);
}

void *ProxyLinker::CallDoDlsym(void *handle, const char *symbol) {
    return dlsym(handle, symbol);
}

#endif

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
#ifdef __LP64__
    #if __ANDROID_API__ >= __ANDROID_API_R__
        #define DLERROR_BUFFER_OFFSET 240
    #elif __ANDROID_API__ == __ANDROID_API_Q__
        #define DLERROR_BUFFER_OFFSET 176
    #elif __ANDROID_API__ >= __ANDROID_API_O__
        #define DLERROR_BUFFER_OFFSET 2304
    #elif __ANDROID_API__ >= __ANDROID_API_N__
        #define DLERROR_BUFFER_OFFSET 2480
    #elif __ANDROID_API__ >= __ANDROID_API_M__
        #define DLERROR_BUFFER_OFFSET 2472
    #else
        #define DLERROR_BUFFER_OFFSET 168
    #endif
#else
    #if __ANDROID_API__ >= __ANDROID_API_R__
        #define DLERROR_BUFFER_OFFSET 144
    #elif __ANDROID_API__ == __ANDROID_API_Q__
        #define DLERROR_BUFFER_OFFSET 92
    #elif __ANDROID_API__ >= __ANDROID_API_O__
        #define DLERROR_BUFFER_OFFSET 1156
    #elif __ANDROID_API__ >= __ANDROID_API_N__
        #define DLERROR_BUFFER_OFFSET 1244
    #elif __ANDROID_API__ >= __ANDROID_API_M__
        #define DLERROR_BUFFER_OFFSET 1220
    #else
        #define DLERROR_BUFFER_OFFSET 68
    #endif

#endif
#define DLERROR_BUFFER_SIZE 512
    void *pthread_internal_t_ptr = GetTls()[1];
    char *buffer = (char *) pthread_internal_t_ptr + DLERROR_BUFFER_OFFSET;
    strlcpy(buffer, msg, DLERROR_BUFFER_SIZE);
    if (detail != nullptr) {
        strlcat(buffer, ": ", DLERROR_BUFFER_SIZE);
        strlcat(buffer, detail, DLERROR_BUFFER_SIZE);
    }
#if __ANDROID_API__ >= __ANDROID_API_Q__
    char *current_dlerror = buffer - sizeof(void *);
    current_dlerror = buffer;
#else
    char **dlerror_slot = reinterpret_cast<char **>(&GetTls()[6]);
    *dlerror_slot = buffer;
#endif
}

void **ProxyLinker::GetTls() {
    return __get_tls();
//    static void **tls_ptr = nullptr;
//    if (tls_ptr == nullptr) {
//        soinfo *libc = FindSoinfoByName("libc.so");
//        auto call_tls = reinterpret_cast<void **(*)()>(libc->find_export_symbol_address("__get_tls"));
//        CHECK(call_tls);
//        tls_ptr = call_tls();
//    }
//    return tls_ptr;
}

soinfo *ProxyLinker::FindSoinfoByName(const char *name) {
    soinfo *si = solist_ptr;

    do {
        if (si->get_soname() != nullptr) {
            if (strcmp(name, si->get_soname()) == 0) {
                return si;
            }
        }
    } while ((si = si->next) != nullptr);
    return nullptr;
}

soinfo *ProxyLinker::FindSoinfoByPath(const char *path) {
    soinfo *si = solist_ptr;

    do {
        if (si->get_soname() != nullptr) {
            if (strstr(si->get_realpath(), path) != nullptr) {
                return si;
            }
        }
    } while ((si = si->next) != nullptr);
    return nullptr;
}

soinfo *ProxyLinker::FindContainingLibrary(const void *p) {
    soinfo *si = solist_ptr;
    ElfW(Addr) address = reinterpret_cast<ElfW(Addr)>(untag_address(p));
    do {
        if (address >= si->base && address - si->base < si->size) {
            return si;
        }
    } while ((si = si->next) != nullptr);
    return nullptr;
}

void *ProxyLinker::FindSymbolByDlsym(soinfo *si, const char *name) {
    void *result;

#if __ANDROID_API__ >= __ANDROID_API_N__
    bool success = CallDoDlsym(reinterpret_cast<void *>(si->get_handle()), name, nullptr, reinterpret_cast<void *>(si->load_bias), &result);
    result = success ? result : nullptr;
#else
    result = CallDoDlsym(si, name);
#endif
    LOGV("dlsym soinfo name: %s, symbol: %s, address: %p", si->get_soname() == nullptr ? "(null)" : si->get_soname(), name, result);
    return result;
}

soinfo *ProxyLinker::GetLinkerSoinfo() {
    return linker_soinfo_ptr;
}

template<typename F>
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
soinfo_list_t ProxyLinker::GetGlobalGroupOfLowVersion() {
    soinfo_list_t global_group;
    for (soinfo *si = solist_ptr; si != nullptr; si = si->next) {
        if ((si->get_dt_flags_1() & DF_1_GLOBAL) != 0) {
            global_group.push_back(si);
        }
    }
    return global_group;
}

soinfo_list_t ProxyLinker::GetSoinfoGlobalGroup(soinfo *root) {
#if __ANDROID_API__ >= __ANDROID_API_N__
    soinfo_list_t global_group = root->get_primary_namespace()->get_global_group();
#else
    soinfo_list_t global_group = GetGlobalGroupOfLowVersion();
#endif
    return global_group;
}

static soinfo_list_t GetSoinfoLocalGroup(soinfo *root) {
    soinfo_list_t local_group;
#if __ANDROID_API__ >= __ANDROID_API_N__
    android_namespace_t *local_group_ns = root->get_primary_namespace();
    walk_dependencies_tree(root, [&](soinfo *child) {
        if (local_group_ns->is_accessible(child)) {
            local_group.push_back(child);
            return kWalkContinue;
        } else {
            return kWalkSkip;
        }
    });
#else
    walk_dependencies_tree(root, [&](soinfo *child) {
        local_group.push_back(child);
        return kWalkContinue;
    });
#endif
    return local_group;
}


/*
 * Android >= 6.0 添加DF_1_GLOBAL标志即可
 * 7.0以上需要将soinfo 添加到所有命名空间
 * 低版本需要把soinfo添加进 g_ld_preloads
 * Android 7.0以下添加进全局组后拥有 RTLD_GLOBAL标志,这会导致在全局库中使用dlsym无法查找到符号
 * */
void ProxyLinker::AddSoinfoToGlobal(soinfo *si) {
    if (si == nullptr) {
        return;
    }

    linker_block_protect_all(PROT_READ | PROT_WRITE);
#if __ANDROID_API__ >= __ANDROID_API_M__
    si->set_dt_flags_1(si->dt_flags_1_ | DF_1_GLOBAL);
#else
    for (int i = 0; i < 9; i++) {
        if (g_ld_preloads_ptr[i] == nullptr) {
            g_ld_preloads_ptr[i] = si;
            break;
        }
    }
#endif

#if __ANDROID_API__ >= __ANDROID_API_N__
    void *main_handle = dlopen(nullptr, 0);
    ProxyLinker::ChangeSoinfoOfNamespace(si, ProxyLinker::SoinfoFromHandle(main_handle)->get_primary_namespace());
    for (const auto np :ProxyLinker::GetAllNamespace(true)) {
        if (si->primary_namespace_ != np) {
            np->add_soinfo(si);
            si->add_secondary_namespace(np);
        }
    }
#endif
    linker_block_protect_all(PROT_READ);
}

int *ProxyLinker::GetGLdDebugVerbosity() {
    return g_ld_debug_verbosity_ptr;
}

/*
 * 6.0以下重写链接
 * */
bool ProxyLinker::RelinkSoinfoImplOfLowVersion(soinfo *si) {
//	bool soinfo::LinkImage(const android_dlextinfo* extinfo);
//	static bool soinfo_link_image(soinfo* si, const android_dlextinfo* extinfo)
    return reinterpret_cast<bool (*)(soinfo *, const android_dlextinfo *)>(link_image_ptr)(si, nullptr);
}

/* RelinkSoinfoImplOfHighVersion
 * 6.0及以上重新链接
 * */
bool ProxyLinker::RelinkSoinfoImplOfHighVersion(soinfo *si) {
    bool linked = false;
#if __ANDROID_API__ < __ANDROID_API_M__
    return linked;
#else
    soinfo_list_t local_group = GetSoinfoLocalGroup(si);
    soinfo_list_t global_group = GetSoinfoGlobalGroup(si);

#if __ANDROID_API__ >= __ANDROID_API_R__
    SymbolLookupList lookup_list(global_group, local_group);
    soinfo *local_group_root = local_group.front();
#endif

    linked = local_group.visit([&](soinfo *lib) {
#if __ANDROID_API__ >= __ANDROID_API_N__
        if (!lib->is_linked() && lib->get_primary_namespace() == si->get_primary_namespace()) {
#else
            if (!lib->is_linked()) {
#endif
#if __ANDROID_API__ >= __ANDROID_API_Q__
            const android_dlextinfo *link_extinfo = nullptr;
            size_t relro_fd_offset = 0;
#endif

#if __ANDROID_API__ >= __ANDROID_API_R__
            //			bool soinfo::link_image(const SymbolLookupList& lookup_list, soinfo* local_group_root,
            //									const android_dlextinfo* extinfo, size_t* relro_fd_offset)

            lookup_list.set_dt_symbolic_lib(lib->has_DT_SYMBOLIC ? lib : nullptr);
            return reinterpret_cast<bool (*)(soinfo *, const SymbolLookupList &, soinfo *, const android_dlextinfo *,
                                             size_t *)>(link_image_ptr)(lib, lookup_list, local_group_root, link_extinfo,
                                                                        &relro_fd_offset);
#elif __ANDROID_API__ == __ANDROID_API_Q__
            //			bool soinfo::link_image(const soinfo_list_t& global_group, const soinfo_list_t& local_group,
            //                  const android_dlextinfo* extinfo, size_t* relro_fd_offset)

            return reinterpret_cast<bool (*)(soinfo *, const soinfo_list_t &, const soinfo_list_t &, const android_dlextinfo *, size_t *)>(link_image_ptr)(lib, global_group,
                                                                                                                                                           local_group,
                                                                                                                                                           link_extinfo,
                                                                                                                                                           &relro_fd_offset);
#elif __ANDROID_API__ >= __ANDROID_API_M__
            //			bool soinfo::link_image(const soinfo_list_t &global_group, const soinfo_list_t &local_group,
            //									const android_dlextinfo *extinfo)

            return reinterpret_cast<bool (*)(soinfo *, const soinfo_list_t &, const soinfo_list_t &, const android_dlextinfo *)>(link_image_ptr)(lib, global_group, local_group,
                                                                                                                                                 nullptr);
#endif
        }
        return true;
    });
#endif
    return linked;
}

static int unprotect_rel_data(gaddress start, gaddress end, int port) {
    int error = mprotect(GSIZE_TO_POINTER(start), end - start, port);
    if (error < 0) {
        LOGE("unprotect rel offset error: %d, port: %d ", error, port);
    }
    return error;
}

static PageProtect *unprotect_library(soinfo *si, const char *name) {
    // 当已经加载后程序头部表与原先文件的段不一致了,还是要手动解析重定位位置
    // 以下并未完全处理重定位项
//#ifdef USE_RELA
//	if (si->plt_rela_ != nullptr) {
//		gaddress start = PAGE_START(si->plt_rela_->r_offset + si->load_bias);
//		gaddress end = PAGE_END((si->plt_rela_ + si->plt_rela_count_ - 1)->r_offset + si->load_bias);
//		if (unprotect_rel_data(start, end, PROT_READ | PROT_WRITE) < 0) {
//			return nullptr;
//		}
//	}
//
//	if (si->rela_ != nullptr){
//		gaddress start = PAGE_START(si->rela_->r_offset + si->load_bias);
//		gaddress end = PAGE_END((si->rela_ + si->rela_count_ - 1)->r_offset + si->load_bias);
//		if (unprotect_rel_data(start, end, PROT_READ | PROT_WRITE) < 0) {
//			return nullptr;
//		}
//	}
//#else
//	if (si->plt_rel_ != nullptr) {
//		gaddress start = PAGE_START(si->plt_rel_->r_offset + si->load_bias);
//		gaddress end = PAGE_END((si->plt_rel_ + si->plt_rel_count_ - 1)->r_offset + si->load_bias);
//		if (unprotect_rel_data(start, end, PROT_READ | PROT_WRITE) < 0) {
//			return nullptr;
//		}
//	}
//
//	if (si->rel_ != nullptr) {
//		gaddress start = PAGE_START(si->rel_->r_offset + si->load_bias);
//		gaddress end = PAGE_END((si->rel_ + si->rel_count_ - 1)->r_offset + si->load_bias);
//		if (unprotect_rel_data(start, end, PROT_READ | PROT_WRITE) < 0) {
//			return nullptr;
//		}
//	}
//#endif

    // 简单修改所有加载段
    MapsUtil util;
    if (!util.GetLibraryProtect(name)) {
        return nullptr;
    }
    if (!util.UnlockPageProtect()) {
        return nullptr;
    }
    return util.CopyProtect();
}


static void symbol_detect(soinfo *si, symbol_observed *symbols, size_t len, bool first) {
#ifdef USE_RELA
    ElfW(Rela) *start = si->plt_rela_ == nullptr ? si->rela_ : si->plt_rela_;
    ElfW(Rela) *end = si->plt_rela_ == nullptr ? si->rela_ + si->rela_count_ : si->plt_rela_ + si->plt_rela_count_;
#else
    ElfW(Rel) *start = si->plt_rel_ == nullptr ? si->rel_ : si->plt_rel_;
    ElfW(Rel) *end = si->plt_rel_ == nullptr ? si->rel_ + si->rel_count_ : si->plt_rel_ + si->plt_rel_count_;
#endif
    if (first) {
        int index = 0;
        for (; start < end; start++, index++) {
            const char *symbol_name = si->strtab_ + (si->symtab_ + R_SYM(start->r_info))->st_name;
            for (int i = 0; i < len; ++i) {
                if (symbols[i].index == 0 && strcmp(symbols[i].name, symbol_name) == 0) {
                    symbols[i].index = index;
                    symbols[i].old_address = *(reinterpret_cast<gsize *>(GSIZE_TO_POINTER(start->r_offset + si->load_bias)));
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < len; ++i) {
            if (symbols[i].index == 0) {
                continue;
            }
            symbols[i].new_address = *reinterpret_cast<gsize *>(GSIZE_TO_POINTER((start + symbols[i].index)->r_offset + si->load_bias));
        }
    }
}

bool ProxyLinker::ManualRelinkLibraries(soinfo *global, const VarLengthObject<const char *> *vars) {
    std::vector<std::string> filters;

    return ProxyLinker::ManualRelinkLibraries(global, vars, filters);
}

bool ProxyLinker::ManualRelinkLibraries(soinfo *global, const VarLengthObject<const char *> *vars, std::vector<std::string> &filters) {
    if (__predict_false(global == nullptr) || __predict_false(vars == nullptr)) {
        return false;
    }
    VarLengthRef<symbol_relocation> rels(global->get_global_soinfo_export_symbols(filters));

    if (rels.data == nullptr) {
        LOGW("Function symbols not exported by the global library : %s", global->get_soname() == nullptr ? "(null)" : global->get_soname());
        return false;
    }
    bool success = true;
    for (int i = 0; i < vars->len; ++i) {
        soinfo *child = ProxyLinker::FindSoinfoByName(vars->elements[i]);
        if (child == nullptr) {
            LOGW("The specified so was not found: %s", vars->elements[i]);
        } else {
            success &= ProxyLinker::ManualRelinkLibrary(rels.data, child);
        }
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
    symbol_relocations *rels = global->get_global_soinfo_export_symbols(filters);
    if (rels == nullptr) {
        LOGW("Function symbols not exported by the global library : %s",
             global->get_soname() == nullptr ? "(null)" : global->get_soname());
        return false;
    }
    return ProxyLinker::ManualRelinkLibrary(rels, child);
}


bool ProxyLinker::ManualRelinkLibrary(symbol_relocations *rels, soinfo *child) {
    if (rels == nullptr || __predict_false(child == nullptr)) {
        return false;
    }
    ScopedPthreadMutexLocker locker(g_dl_mutex_ptr);
    return child->again_process_relocation(rels);
}

/*
 * 调用系统重定位会出现各种问题,废弃使用
 * */
bool ProxyLinker::SystemRelinkLibrary(soinfo *so) {
    bool success;
    if (so == nullptr) {
        return false;
    }
    if (link_image_ptr == nullptr) {
        LOGE("This device does not support the system relink, not found link_image impl function");
        return false;
    }
    ScopedPthreadMutexLocker locker(g_dl_mutex_ptr);
    linker_block_protect_all(PROT_READ | PROT_WRITE);
    so->set_unlinked();
    std::string str = soinfo_to_string(so);
    // 重新dlopen出错,因为目前进程已经存在该so就不会在走ElfRead
    // 5.0查找会修改linker的数据段,因此还要解保护linker
    MapsUtil util(so->get_realpath());
    if (!util.Found()) {
        return false;
    }
    util.UnlockPageProtect();
#if __ANDROID_API__ < __ANDROID_API_M__
    success = RelinkSoinfoImplOfLowVersion(so);
#else
    success = RelinkSoinfoImplOfHighVersion(so);
#endif
    if (success) {
        so->set_linked();
    }
    util.RecoveryPageProtect();
    LOGV("The system relink library: %s, result: %s", so->get_soname(), success ? "true" : "false");
    linker_block_protect_all(PROT_READ);
    return success;
}

bool ProxyLinker::SystemRelinkLibraries(const VarLengthObject<const char *> *libs) {
    if (__predict_false(libs == nullptr)) {
        return false;
    }
    bool success = true;
    for (int i = 0; i < libs->len; ++i) {
        soinfo *so = ProxyLinker::FindSoinfoByName(libs->elements[i]);
        if (so == nullptr) {
            LOGW("The specified so was not found: %s", libs->elements[i]);
        } else {
            success &= ProxyLinker::SystemRelinkLibrary(so);
        }
    }
    return success;
}


void *ProxyLinker::GetDlopenOrDlsymAddress(bool dlopen_type) {
    return dlopen_type ? reinterpret_cast<void *>(ProxyLinker::CallDoDlopen) : reinterpret_cast<void *>(ProxyLinker::CallDoDlsym);
}

void ProxyLinker::Init() {
    if (solist_ptr != nullptr) {
        return;
    }
#ifdef __LP64__
    const char *lib = "/linker64";
    const char* create_namespace_name = "__dl__Z16create_namespacePKvPKcS2_S2_mS2_P19android_namespace_t";
#else
    const char *lib = "/linker";
    const char *create_namespace_name = "__dl__Z16create_namespacePKvPKcS2_S2_yS2_P19android_namespace_t";
#endif
// __dl__Z16create_namespacePKvPKcS2_S2_yS2_P19android_namespace_t
// __dl__Z16create_namespacePKvPKcS2_S2_mS2_P19android_namespace_t
#define COMMON_SYMBOLS "__dl__ZL6solist", "__dl_g_ld_debug_verbosity", "__dl__ZL10g_dl_mutex", "__dl__ZL19__linker_dl_err_buf", LINK_IMAGE_NAME

#if __ANDROID_API__ >= __ANDROID_API_N__
    VarLengthRef<gaddress> symbols(ResolveLibrarySymbolsAddress(lib, kInner, 10, COMMON_SYMBOLS, "__dl_g_default_namespace",
                                                                SOINFO_HANDLES_NAME, DO_DLOPEN_NAME, DO_DLSYM_NAME, create_namespace_name));
#elif __ANDROID_API__ == __ANDROID_API_M__
    VarLengthRef<gaddress> symbols(ResolveLibrarySymbolsAddress(lib, kInner, 6, COMMON_SYMBOLS, DO_DLOPEN_NAME));
#else
    VarLengthRef<gaddress> symbols(ResolveLibrarySymbolsAddress(lib, kInner, 7, COMMON_SYMBOLS, "__dl__ZL13g_ld_preloads", DO_DLOPEN_NAME));
#endif

    if (symbols.data == nullptr) {
        LOGE("find solist ... symbols failed.");
        return;
    }
    solist_ptr = *(soinfo **) symbols.data->elements[0];
    g_ld_debug_verbosity_ptr = reinterpret_cast<int *>(symbols.data->elements[1]);
    g_dl_mutex_ptr = reinterpret_cast<pthread_mutex_t *>(symbols.data->elements[2]);
    linker_dl_err_buf_ptr = reinterpret_cast<char *>(symbols.data->elements[3]);
    link_image_ptr = reinterpret_cast<void *>(symbols.data->elements[4]);
    CHECK(solist_ptr);
    CHECK(g_ld_debug_verbosity_ptr);
    CHECK(g_dl_mutex_ptr);
    CHECK(linker_dl_err_buf_ptr);

#if  __ANDROID_API__ >= __ANDROID_API_N__

    g_default_namespace_ptr = (android_namespace_t *) symbols.data->elements[5];
    g_soinfo_handles_map_ptr = (std::unordered_map<uintptr_t, soinfo *> *) symbols.data->elements[6];

    CHECK(g_default_namespace_ptr);
    CHECK(g_soinfo_handles_map_ptr);
    LOGV("find linker soinfo: %p, g_ld_debug_verbosity:%p, g_default_namespace: %p, g_soinfo_handles_map: %p, link_image: %p",
         solist_ptr, g_ld_debug_verbosity_ptr, g_default_namespace_ptr, g_soinfo_handles_map_ptr, link_image_ptr);

#elif __ANDROID_API__ == __ANDROID_API_M__
#else
    g_ld_preloads_ptr = (soinfo **) symbols.data->elements[5];
    CHECK(g_ld_preloads_ptr);
    LOGV("find soinfo: %p, g_ld_preloads: %p, link_image: %p", solist_ptr, g_ld_preloads_ptr, link_image_ptr);
#endif

#if __ANDROID_API__ >= __ANDROID_API_O__
    linker_soinfo_ptr = ProxyLinker::FindSoinfoByName("ld-android.so");
    CHECK(linker_soinfo_ptr);
    dlopen_ptr = reinterpret_cast<void *(*)(const char *, int, const android_dlextinfo *, const void *)>(linker_soinfo_ptr->find_export_symbol_address(
            "__loader_android_dlopen_ext"));
    dlsym_ptr = reinterpret_cast<void *(*)(void *, const char *, const void *)>(linker_soinfo_ptr->find_export_symbol_address("__loader_dlsym"));
    create_namespace_ptr = reinterpret_cast<android_namespace_t *(*)(const char *, const char *, const char *, uint64_t, const char *, android_namespace_t *, const void *)>
    (linker_soinfo_ptr->find_export_symbol_address("__loader_android_create_namespace"));
    LOGV("linker __loader_android_dlopen_ext: %p, __loader_dlsym: %p, __loader_android_create_namespace: %p", dlopen_ptr, dlsym_ptr, create_namespace_ptr);
    CHECK(create_namespace_ptr);
#else
    linker_soinfo_ptr = ProxyLinker::FindSoinfoByName("libdl.so");
    CHECK(linker_soinfo_ptr);
    #if __ANDROID_API__ >= __ANDROID_API_N__
    dlopen_ptr = reinterpret_cast<void *(*)(const char *, int, const android_dlextinfo *, const void *)>(symbols.data->elements[7]);
    dlsym_ptr = reinterpret_cast<bool (*)(void *, const char *, const char *, void *, void **)>(symbols.data->elements[8]);
    create_namespace_ptr = reinterpret_cast<android_namespace_t *(*)(const void *, const char *, const char *, const char *, uint64_t, const char *, android_namespace_t *)>
    (symbols.data->elements[9]);
    CHECK(create_namespace_ptr);
    #else
    dlopen_ptr = reinterpret_cast< void *(*)(const char *, int, const android_dlextinfo *)>(linker_soinfo_ptr->find_export_symbol_address("android_dlopen_ext"));
    dlsym_ptr = reinterpret_cast<void *(*)(void *, const char *)>(linker_soinfo_ptr->find_export_symbol_address("dlsym"));
    #endif
#endif
    CHECK(dlopen_ptr);
    CHECK(dlsym_ptr);
}