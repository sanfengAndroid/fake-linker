//
// Created by beich on 2020/11/15.
//

#include <macros.h>
#include "linker_globals.h"


#ifdef USE_RELA
#define ELF_REL ElfW(Rela)
#else
#define ELF_REL ElfW(Rel)
#endif

static std::vector<std::string> filter_symbols;

template<class T>
static VarLengthObject<T> *collects_to_var_length_object(const std::vector<T> vectors) {
    VarLengthObject<T> *ret = VarLengthObjectAlloc<T>(vectors.size());
    int index = 0;

    for (auto &element: vectors) {
        ret->elements[index++] = element;
    }
    return ret;
}

static soinfo *find_soinfo_by_type(SoinfoParamType type, const void *p, void *caller, int *error_code) {
    soinfo *result = nullptr;
    switch (type) {
        case kSPAddress:
            if (p == nullptr) {
                p = caller;
            }
            result = ProxyLinker::FindContainingLibrary(p);
            break;
        case kSPHandle:
#if __ANDROID_API__ >= __ANDROID_API_N__
            if (p != nullptr) {
                result = ProxyLinker::SoinfoFromHandle(p);
            } else {
                *error_code = kErrorParameterNull;
            }
#else
            *error_code = kErrorApiLevelNotMatch;
#endif
            break;
        case kSPOriginal:
            result = reinterpret_cast<soinfo *>(const_cast<void *>(p));
            break;
        case kSPName:
            result = ProxyLinker::FindSoinfoByName(reinterpret_cast<const char *>(p));
            break;
        default:
            *error_code = kErrorParameterType;
            break;
    }
    if (result == nullptr) {
        *error_code |= kErrorSoinfoNotFound;
    }
    return result;
}

API_PUBLIC void *call_soinfo_function(SoinfoFunType fun_type, SoinfoParamType find_type, const void *find_param,
                                      SoinfoParamType param_type, const void *param, int *error_code) {
    void *result = nullptr;
    soinfo *so;
    void *caller = __builtin_return_address(0);

    auto found = [&]() {
        so = find_soinfo_by_type(find_type, find_param, caller, error_code);
        return so != nullptr;
    };

    *error_code = kErrorNo;
    switch (fun_type) {
        case kSFInquire:
            if (found()) {
                result = so;
            }
            break;
        case kSFInquireAttr:
            if (found()) {
                auto *attr = new SoinfoAttribute();
                attr->soinfo_original = so;
                attr->so_name = so->get_soname();
                attr->real_path = so->get_realpath();
#if __ANDROID_API__ >= __ANDROID_API_N__
                attr->handle = reinterpret_cast<void *>(so->get_handle());
#endif
                attr->base = so->base;
                attr->size = so->size;
                result = attr;
            }
            break;
        case kSFGetHandle:
            if (found()) {
#if __ANDROID_API__ >= __ANDROID_API_N__
                result = reinterpret_cast< void * > (so->get_handle());
#else
                result = so;
#endif
            }
            break;
        case kSFGetName:
            if (found()) {
                result = const_cast<char *>(so->get_soname());
            }
            break;
        case kSFGetRealPath:
            if (found()) {
                result = const_cast<char *>(so->get_realpath());
            }
            break;
        case kSFGetLinker:
            result = ProxyLinker::GetLinkerSoinfo();
            break;
        case kSFGetDlopen:
            result = ProxyLinker::GetDlopenOrDlsymAddress(true);
            break;
        case kSFGetDlsym:
            result = ProxyLinker::GetDlopenOrDlsymAddress(false);
            break;
        case kSFGetImportSymbolAddress:
            if (found()) {
                if (param_type != kSPSymbol) {
                    *error_code = kErrorParameterType;
                } else {
                    result = so->find_import_symbol_address(reinterpret_cast<const char *>(param));
                    if (result == nullptr) {
                        *error_code = kErrorSymbolNotFoundInSoinfo;
                    }
                }
            }
            break;
        case kSFGetExportSymbolAddress:
            if (found()) {
                if (param_type != kSPSymbol) {
                    *error_code = kErrorParameterType;
                } else {
                    result = so->find_export_symbol_address(reinterpret_cast<const char *>(param));
                    if (result == nullptr) {
                        *error_code = kErrorSymbolNotFoundInSoinfo;
                    }
                }
            }
            break;
        case kSFCallDlsym:
            if (found()) {
                if (param_type != kSPSymbol) {
                    *error_code = kErrorParameterType;
                    break;
                }
                result = ProxyLinker::FindSymbolByDlsym(so, reinterpret_cast<const char *>(param));
                if (result == nullptr) {
                    *error_code = kErrorSymbolNotFoundInSoinfo;
                }
            }
            break;
        case kSFGetLinkerSymbol:
            if (find_type != kSPSymbol) {
                *error_code = kErrorParameterType;
                break;
            }
            so = ProxyLinker::GetLinkerSoinfo();
            result = so->find_export_symbol_address(static_cast<const char *>(find_param));
            if (result == nullptr) {
                *error_code = kErrorSymbolNotFoundInSoinfo;
            }
            break;
        case kSFCallDlopen:
            *error_code = kErrorFunctionNotImplemented;
            break;
        default:
            *error_code = kErrorFunctionUndefined;
            break;
    }
    return result;
}

API_PUBLIC void *call_common_function(CommonFunType fun_type, SoinfoParamType find_type, const void *find_param,
                                      SoinfoParamType param_type, const void *param, int *error_code) {
    void *result = nullptr;
    soinfo *so;
    soinfo *child;
    VarLengthObject<const char *> *libs;
    bool success;
    void *caller = __builtin_return_address(0);
    *error_code = kErrorNo;

    auto found = [&]() {
        so = find_soinfo_by_type(find_type, find_param, caller, error_code);
        return so != nullptr;
    };

    switch (fun_type) {
        case kCFAddSoinfoToGlobal:
            if (found()) {
                ProxyLinker::AddSoinfoToGlobal(so);
            }
            break;
        case kCFCallManualRelink:
            if (found()) {
                child = find_soinfo_by_type(param_type, param, caller, error_code);
                if (child != nullptr) {
                    success = ProxyLinker::ManualRelinkLibrary(so, child, filter_symbols);
                    if (!success) {
                        *error_code = kErrorSoinfoRelink;
                    }
                }
            }
            break;
        case kCFCallManualRelinks:
            if (found()) {
                if (param_type != kSPNames) {
                    *error_code = kErrorParameterType;
                    break;
                }
                libs = reinterpret_cast<VarLengthObject<const char *> *>(const_cast<void *>(param));
                success = ProxyLinker::ManualRelinkLibraries(so, libs, filter_symbols);
                VarLengthObjectFree(libs);
                if (!success) {
                    *error_code |= kErrorSoinfoRelink;
                }
            }
            break;
        case kCFAddRelinkFilterSymbol:
            if (find_type != kSPSymbol) {
                *error_code = kErrorParameterType;
                break;
            }
            if (find_param == nullptr) {
                *error_code = kErrorParameterNull;
                break;
            }
            {
                if (std::find(filter_symbols.begin(), filter_symbols.end(), (const char *) find_param) == filter_symbols.end()) {
                    filter_symbols.emplace_back((const char *) find_param);
                }
            }
            break;
        case kCFAddRelinkFilterSymbols:
            if (find_type != kSPSymbol) {
                *error_code = kErrorParameterType;
                break;
            }
            if (find_param == nullptr) {
                *error_code = kErrorParameterNull;
                break;
            }
            libs = reinterpret_cast<VarLengthObject<const char *> *>(const_cast<void *>(find_param));
            for (int i = 0; i < libs->len; ++i) {
                std::string v = libs->elements[i];
                if (std::find(filter_symbols.begin(), filter_symbols.end(), v) == filter_symbols.end()) {
                    filter_symbols.push_back(v);
                }
            }
            VarLengthObjectFree(libs);
            break;
        case kCFRemoveRelinkFilterSymbol:
            if (find_type != kSPSymbol) {
                *error_code = kErrorParameterType;
                break;
            }
            if (find_param == nullptr) {
                *error_code = kErrorParameterNull;
                break;
            }
            {
                auto f = std::find(filter_symbols.begin(), filter_symbols.end(), (const char *) find_param);
                if (f != filter_symbols.end()) {
                    filter_symbols.erase(f);
                }
            }
            break;
        case kCFRemoveRelinkFilterSymbols:
            if (find_type != kSPSymbol) {
                *error_code = kErrorParameterType;
                break;
            }
            if (find_param == nullptr) {
                filter_symbols.clear();
                break;
            }
            libs = reinterpret_cast<VarLengthObject<const char *> *>(const_cast<void *>(find_param));
            for (int i = 0; i < libs->len; ++i) {
                auto f = std::find(filter_symbols.begin(), filter_symbols.end(), libs->elements[i]);
                if (f != filter_symbols.end()) {
                    filter_symbols.erase(f);
                }
            }
            VarLengthObjectFree(libs);
            break;
        default:
            *error_code = kErrorFunctionUndefined;
            break;
    }
    return result;
}


#if __ANDROID_API__ >= __ANDROID_API_N__

static android_namespace_t *find_namespace(NamespaceParamType type, const void *param, void *caller, int *error_code) {
    android_namespace_t *result = nullptr;
    const soinfo *so = nullptr;

    switch (type) {
        case kNPOriginal:
            result = reinterpret_cast<android_namespace_t *>(const_cast<void *>(param));
            break;
        case kNPSoinfo:
            so = reinterpret_cast<const soinfo *>(param);
            break;
        case kNPSoinfoHandle:
            so = find_soinfo_by_type(kSPHandle, param, caller, error_code);
            break;
        case kNPAddress:
            so = find_soinfo_by_type(kSPAddress, param, caller, error_code);
            break;
        case kNPSoinfoName:
            so = find_soinfo_by_type(kSPName, param, caller, error_code);
            break;
        case kNPNamespaceName:
            result = ProxyLinker::FindNamespaceByName(reinterpret_cast<const char *>(param));
            if (result == nullptr) {
                *error_code = kErrorNpName;
            }
            break;
        default:
            *error_code = kErrorParameterType;
            break;
    }
    if (result == nullptr && so != nullptr) {
        result = so->primary_namespace_;
    }
    if (result == nullptr) {
        *error_code |= kErrorNpNotFound;
    }
    return result;
}

static soinfo *find_soinfo(NamespaceParamType type, const void *param, void *caller, int *error_code) {
    soinfo *result = nullptr;
    switch (type) {
        case kNPSoinfo:
            result = reinterpret_cast<soinfo *>(const_cast<void *>(param));
            break;
        case kNPSoinfoHandle:
            result = find_soinfo_by_type(kSPHandle, param, caller, error_code);
            break;
        case kNPAddress:
            result = find_soinfo_by_type(kSPAddress, param, caller, error_code);
            break;
        case kNPSoinfoName:
            result = find_soinfo_by_type(kSPName, param, caller, error_code);
            break;
        default:
            *error_code = kErrorParameterType;
            break;
    }
    return result;
}

API_PUBLIC void *call_namespace_function(NamespaceFunType fun_type, NamespaceParamType find_type, const void
*find_param, NamespaceParamType param_type, const void *param, int *error_code) {
    void *result = nullptr;
    android_namespace_t *np = nullptr;
    int size = 0;
    soinfo *so;

    void *caller = __builtin_return_address(0);

    *error_code = kErrorNo;
    auto found = [&](NamespaceParamType type, bool param_null) {
        np = find_namespace(find_type, find_param, caller, error_code);
        if (np != nullptr) {
            if (type != kNPNull) {
                if (type != param_type) {
                    *error_code |= kErrorParameterType;
                    return false;
                }
            }
            if (param_null) {
                if (param == nullptr) {
                    *error_code |= kErrorParameterNull;
                    return false;
                }
            }
            return true;
        }
        return false;
    };

    switch (fun_type) {
        case kNFInquire:
            result = find_namespace(find_type, find_param, caller, error_code);
            break;
        case kNFInquireAll:
            result = collects_to_var_length_object<android_namespace_t *>(ProxyLinker::GetAllNamespace(true));
            break;
        case kNFInquireSoinfo:
            if (found(kNPNull, false)) {
                size = np->soinfo_list().size();
                VarLengthObject<soinfo *> *vars = VarLengthObjectAlloc<soinfo *>(size);
                int index = 0;
                np->soinfo_list().for_each([&](soinfo *_so) {
                    vars->elements[index++] = _so;
                });
                result = vars;
            }
            break;
        case kNFInquireGlobalSoinfos:
            if (found(kNPNull, false)) {
                soinfo_list_t list = np->get_global_group();
                VarLengthObject<soinfo *> *vars = VarLengthObjectAlloc<soinfo *>(list.size());
                int index = 0;
                list.for_each([&](soinfo *_so) {
                    vars->elements[index++] = _so;
                });
                result = vars;
            }
            break;
        case kNFAddGlobalSoinfoToNamespace:
            if (found(kNPNull, false)) {
                soinfo *global = find_soinfo(param_type, param, caller, error_code);
                if (global != nullptr) {
                    if (!ProxyLinker::AddGlobalSoinfoToNamespace(global, np)) {
                        *error_code = kErrorExec;
                    }
                }
            }
            break;
        case kNFInquireLinked:
#if __ANDROID_API__ >= __ANDROID_API_O__
            if (found(kNPNull, false)) {
                // 注意链接命名空间是拷贝赋值,不是返回指针
                result = collects_to_var_length_object(np->linked_namespaces_);
            }
#else
            *error_code = kErrorApiLevelNotMatch;
#endif
            break;
        case kNFAddSoinfoToNamespace:
            if (found(kNPNull, false)) {
                so = find_soinfo(param_type, param, caller, error_code);
                if (so != nullptr) {
                    np->add_soinfo(so);
                }
            }
            break;
        case kNFAddSoinfoToWhiteList:
        case kNFAddSoinfosToWhiteList:
#if __ANDROID_API__ >= __ANDROID_API_Q__
            if (found(kNPSoinfoName, true)) {
                if (fun_type == kNFAddSoinfoToWhiteList) {
                    np->whitelisted_libs_.emplace_back(static_cast<const char *>(param));
                } else {
                    const auto *vars = reinterpret_cast<const VarLengthObject<const char *> *>(param);
                    for (int i = 0; i < vars->len; ++i) {
                        np->whitelisted_libs_.emplace_back(vars->elements[i]);
                    }
                }
            }
#else
            *error_code = kErrorApiLevelNotMatch;
#endif
            break;
        case kNFAddPathToLdLibraryPath:
        case kNFAddPathsToLdLibraryPath:
            if (found(kNPPath, true)) {
                if (fun_type == kNFAddPathToLdLibraryPath) {
                    np->ld_library_paths_.emplace_back(static_cast<const char *>(param));
                } else {
                    const auto *vars = reinterpret_cast<const VarLengthObject<const char *> *>(param);
                    for (int i = 0; i < vars->len; ++i) {
                        np->ld_library_paths_.emplace_back(vars->elements[i]);
                    }
                }
            }
            break;
        case kNFAddPathToDefaultLibraryPath:
        case kNFAddPathsToDefaultLibraryPath:
            if (found(kNPPath, true)) {
                if (fun_type == kNFAddPathToDefaultLibraryPath) {
                    np->default_library_paths_.emplace_back(static_cast<const char *>(param));
                } else {
                    const auto *vars = reinterpret_cast<const VarLengthObject<const char *> *>(param);
                    for (int i = 0; i < vars->len; ++i) {
                        np->default_library_paths_.emplace_back(vars->elements[i]);
                    }
                }
            }
            break;
        case kNFAddPathToPermittedPath:
        case kNFAddPathsToPermittedPath:
            if (found(kNPPath, true)) {
                if (fun_type == kNFAddPathToPermittedPath) {
                    np->permitted_paths_.emplace_back(static_cast<const char *>(param));
                } else {
                    const auto *vars = reinterpret_cast<const VarLengthObject<const char *> *>(param);
                    for (int i = 0; i < vars->len; ++i) {
                        np->permitted_paths_.emplace_back(vars->elements[i]);
                    }
                }
            }
            break;
        case kNFAddLinkedNamespace:
        case kNFAddLinkedNamespaces:
#if __ANDROID_API__ >= __ANDROID_API_O__
            if (found(kNPLinkedNamespace, true)) {
                if (fun_type == kNFAddLinkedNamespace) {
                    const auto *linked = reinterpret_cast<const android_namespace_link_t *>(param);
                    np->linked_namespaces_.push_back(*linked);
                } else {
                    const auto *vars = reinterpret_cast<const VarLengthObject<android_namespace_link_t *> *>(param);
                    for (int i = 0; i < vars->len; ++i) {
                        np->linked_namespaces_.push_back(*vars->elements[i]);
                    }
                }
            }
#else
            *error_code = kErrorApiLevelNotMatch;
#endif
            break;
        case kNFAddSecondNamespace:
            so = find_soinfo(find_type, find_param, caller, error_code);
            if (so != nullptr) {
                if (param_type != kNPOriginal) {
                    *error_code = kErrorParameterType;
                    break;
                }
                if (param == nullptr) {
                    *error_code = kErrorParameterNull;
                    break;
                }
                np = reinterpret_cast<android_namespace_t *>(const_cast<void *>(param));
                so->add_secondary_namespace(np);
            }
            break;
        case kNPChangeSoinfoNamespace:
            so = find_soinfo(find_type, find_param, caller, error_code);
            if (so != nullptr) {
                if (param != nullptr) {
                    np = find_namespace(param_type, param, caller, error_code);
                    if (np == nullptr) {
                        break;
                    }
                }
                ProxyLinker::ChangeSoinfoOfNamespace(so, np);
            }
            break;
        default:
            *error_code = kErrorFunctionUndefined;
            break;
    }
    return result;
}

#endif