//
// Created by beich on 2020/11/5.
//

#include "linker_namespaces.h"

#include <dlfcn.h>

#include "linker_common_types.h"
#include "linker_util.h"
#include "linker_globals.h"

bool android_namespace_t::is_accessible(const std::string &file) {
    if (!is_isolated_) {
        return true;
    }
#if __ANDROID_API__ >= __ANDROID_API_Q__
    if (!whitelisted_libs_.empty()) {
        const char *lib_name = basename(file.c_str());
        if (std::find(whitelisted_libs_.begin(), whitelisted_libs_.end(),
                      lib_name) == whitelisted_libs_.end()) {
            return false;
        }
    }
#endif

    for (const auto &dir : ld_library_paths_) {
        if (file_is_in_dir(file, dir)) {
            return true;
        }
    }

    for (const auto &dir : default_library_paths_) {
        if (file_is_in_dir(file, dir)) {
            return true;
        }
    }

    for (const auto &dir : permitted_paths_) {
        if (file_is_under_dir(file, dir)) {
            return true;
        }
    }

    return false;
}

bool android_namespace_t::is_accessible(soinfo *s) {
    auto is_accessible_ftor = [this](soinfo *si, bool allow_secondary) {
        if (!si->has_min_version(3)) {
            return false;
        }
#if __ANDROID_API__ >= __ANDROID_API_N__
        if (si->get_primary_namespace() == this) {
            return true;
        }
#endif
        // When we're looking up symbols, we want to search libraries from the same namespace (whether
        // the namespace membership is primary or secondary), but we also want to search the immediate
        // dependencies of libraries in our namespace. (e.g. Supposing that libapp.so -> libandroid.so
        // crosses a namespace boundary, we want to search libandroid.so but not any of libandroid.so's
        // dependencies).
        //
        // Some libraries may be present in this namespace via the secondary namespace list:
        //  - the executable
        //  - LD_PRELOAD and DF_1_GLOBAL libraries
        //  - libraries inherited during dynamic namespace creation (e.g. because of
        //    RTLD_GLOBAL / DF_1_GLOBAL / ANDROID_NAMESPACE_TYPE_SHARED)
        //
        // When a library's membership is secondary, we want to search its symbols, but not the symbols
        // of its dependencies. The executable may depend on internal system libraries which should not
        // be searched.
        if (allow_secondary) {
#if __ANDROID_API__ >= __ANDROID_API_N__
            const android_namespace_list_t &secondary_namespaces = si->get_secondary_namespaces();
            if (secondary_namespaces.find(this) != secondary_namespaces.end()) {
                return true;
            }
#endif
        }
        return false;
    };

    if (is_accessible_ftor(s, true)) {
        return true;
    }

    return !s->get_parents().visit([&](soinfo *si) {
        return !is_accessible_ftor(si, false);
    });
}

// the global group for relocation. Not every RTLD_GLOBAL
// library is included in this group for backwards-compatibility
// reasons.
//
// This group consists of the main executable, LD_PRELOADs
// and libraries with the DF_1_GLOBAL flag set.
soinfo_list_t android_namespace_t::get_global_group() {
    soinfo_list_t global_group;
    soinfo_list().for_each([&](soinfo *si) {
        if ((si->get_dt_flags_1() & DF_1_GLOBAL) != 0) {
            global_group.push_back(si);
        }
    });

    return global_group;
}

// This function provides a list of libraries to be shared
// by the namespace. For the default namespace this is the global
// group (see get_global_group). For all others this is a group
// of RTLD_GLOBAL libraries (which includes the global group from
// the default namespace).

soinfo_list_t android_namespace_t::get_shared_group() {
#if __ANDROID_API__ >= __ANDROID_API_N__
    if (this == ProxyLinker::GetDefaultNamespace()) {
        return get_global_group();
    }

    soinfo_list_t shared_group;
    soinfo_list().for_each([&](soinfo *si) {
        if ((si->get_rtld_flags() & RTLD_GLOBAL) != 0) {
            shared_group.push_back(si);
        }
    });

    return shared_group;
#endif
    CHECK(false);
}
