//
// Created by beich on 2020/11/5.
//

#include "linker_namespaces.h"

#include <dlfcn.h>
#include <libgen.h>

#include <fakelinker/type.h>

#include "linker_common_types.h"
#include "linker_globals.h"
#include "linker_util.h"


ANDROID_GE_O
android_namespace_link_t::android_namespace_link_t(android_namespace_t *linked_namespace,
                                                   const std::unordered_set<std::string> &shared_lib_sonames) :
    linked_namespace_(linked_namespace), shared_lib_sonames_(shared_lib_sonames) {}

#define AN_FUN(Ret, Name) Ret (*Name)(android_namespace_t * thiz)
#define CALL_MEMBER(Name) npTable.Name(this)

struct AndroidNamespaceFunTable {
  AN_FUN(const char *&, name_);

  AN_FUN(bool &, is_isolated_);

  ANDROID_GE_R AN_FUN(bool &, is_also_used_as_anonymous_);

  ANDROID_GE_O AN_FUN(bool &, is_greylist_enabled_);

  AN_FUN(std::vector<std::string> &, ld_library_paths_);

  AN_FUN(std::vector<std::string> &, default_library_paths_);

  AN_FUN(std::vector<std::string> &, permitted_paths_);

  ANDROID_GE_Q AN_FUN(std::vector<std::string> &, whitelisted_libs_);

  ANDROID_GE_P AN_FUN(std::vector<android_namespace_link_t_P> &, linked_namespaces_P);

  ANDROID_GE_O AN_FUN(std::vector<android_namespace_link_t_O> &, linked_namespaces_);

  AN_FUN(soinfo_list_t &, soinfo_list_);
  ANDROID_GE_T AN_FUN(soinfo_list_t_T &, soinfo_list_T);

  void (*setName)(android_namespace_t *an, const char *name);
};

static AndroidNamespaceFunTable npTable;

#define NP_MEMBER_REF_WRAP(Type, Name)                                                                                 \
  npTable.Name = [](android_namespace_t *thiz) -> member_ref_type_trait<decltype(&Type::Name)>::type {                 \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

#define NP_MEMBER_REF_VER_WRAP(Type, Name, Ver)                                                                        \
  npTable.Name##Ver = [](android_namespace_t *thiz) -> member_ref_type_trait<decltype(&Type::Name)>::type {            \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

#define NP_MEMBER_NULL(Type, Name) npTable.Name = nullptr;

void android_namespace_t::Init() {
  if (android_api < __ANDROID_API_N__) {
    return;
  }
  if (android_api >= __ANDROID_API_T__) {
    NP_MEMBER_NULL(android_namespace_t_T, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, is_isolated_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, is_greylist_enabled_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, permitted_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_T, whitelisted_libs_);
    NP_MEMBER_REF_VER_WRAP(android_namespace_t_T, linked_namespaces_, P);
    NP_MEMBER_REF_VER_WRAP(android_namespace_t_T, soinfo_list_, T);
  } else if (android_api >= __ANDROID_API_R__) {
    NP_MEMBER_NULL(android_namespace_t_R, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, is_isolated_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, is_greylist_enabled_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, permitted_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, whitelisted_libs_);
    NP_MEMBER_REF_VER_WRAP(android_namespace_t_R, linked_namespaces_, P);
    NP_MEMBER_REF_WRAP(android_namespace_t_R, soinfo_list_);
    npTable.setName = [](android_namespace_t *thiz, const char *name) {
      reinterpret_cast<android_namespace_t_R *>(thiz)->name_ = name;
    };
  } else if (android_api >= __ANDROID_API_Q__) {
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, is_isolated_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, is_greylist_enabled_);
    NP_MEMBER_NULL(android_namespace_t_Q, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, permitted_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, whitelisted_libs_);
    NP_MEMBER_REF_VER_WRAP(android_namespace_t_Q, linked_namespaces_, P);
    NP_MEMBER_REF_WRAP(android_namespace_t_Q, soinfo_list_);
  } else if (android_api >= __ANDROID_API_P__) {
    NP_MEMBER_REF_WRAP(android_namespace_t_P, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, is_isolated_);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, is_greylist_enabled_);
    NP_MEMBER_NULL(android_namespace_t_P, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, permitted_paths_);
    NP_MEMBER_REF_VER_WRAP(android_namespace_t_P, linked_namespaces_, P);
    NP_MEMBER_REF_WRAP(android_namespace_t_P, soinfo_list_);
  } else if (android_api >= __ANDROID_API_O__) {
    NP_MEMBER_REF_WRAP(android_namespace_t_O, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, is_isolated_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, is_greylist_enabled_);
    NP_MEMBER_NULL(android_namespace_t_O, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, permitted_paths_);
    NP_MEMBER_NULL(android_namespace_t_O, whitelisted_libs_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, linked_namespaces_);
    NP_MEMBER_REF_WRAP(android_namespace_t_O, soinfo_list_);
  } else {
    NP_MEMBER_REF_WRAP(android_namespace_t_N, name_);
    NP_MEMBER_REF_WRAP(android_namespace_t_N, is_isolated_);
    NP_MEMBER_NULL(android_namespace_t_N, is_greylist_enabled_);
    NP_MEMBER_NULL(android_namespace_t_N, is_also_used_as_anonymous_);
    NP_MEMBER_REF_WRAP(android_namespace_t_N, ld_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_N, default_library_paths_);
    NP_MEMBER_REF_WRAP(android_namespace_t_N, permitted_paths_);
    NP_MEMBER_NULL(android_namespace_t_N, whitelisted_libs_);
    NP_MEMBER_NULL(android_namespace_t_N, linked_namespaces_);
    NP_MEMBER_REF_WRAP(android_namespace_t_N, soinfo_list_);
  }
}

android_namespace_t::android_namespace_t() { CALL_MEMBER(is_isolated_) = false; }

const char *android_namespace_t::get_name() {
  if (android_api >= __ANDROID_API_R__) {
    return reinterpret_cast<android_namespace_t_R *>(this)->name_.c_str();
  }
  return CALL_MEMBER(name_);
}

void android_namespace_t::set_name(const char *name) {
  if (android_api >= __ANDROID_API_R__) {
    npTable.setName(this, name);
  } else {
    CALL_MEMBER(name_) = name;
  }
}

bool android_namespace_t::is_isolated() { return CALL_MEMBER(is_isolated_); }

void android_namespace_t::set_isolated(bool isolated) {
  LinkerBlockLock lock;
  CALL_MEMBER(is_isolated_) = isolated;
}

ANDROID_GE_O bool android_namespace_t::is_greylist_enabled() { return CALL_MEMBER(is_greylist_enabled_); }

ANDROID_GE_O void android_namespace_t::set_greylist_enabled(bool enabled) {
  LinkerBlockLock lock;
  CALL_MEMBER(is_greylist_enabled_) = enabled;
}

ANDROID_GE_P const std::vector<android_namespace_link_t_P> &android_namespace_t::linked_namespaceP() {
  return CALL_MEMBER(linked_namespaces_P);
}

ANDROID_GE_O const std::vector<android_namespace_link_t_O> &android_namespace_t::linked_namespacesO() {
  return CALL_MEMBER(linked_namespaces_);
}

ANDROID_GE_O void android_namespace_t::add_linked_namespace(android_namespace_t *linked_namespace,
                                                            const std::unordered_set<std::string> &shared_lib_sonames,
                                                            bool allow_all_shared_libs) {
  LinkerBlockLock lock;
  if (android_api <= __ANDROID_API_O_MR1__) {
    auto &linked = CALL_MEMBER(linked_namespaces_);
    linked.emplace_back(linked_namespace, shared_lib_sonames);
  } else {
    auto &linked = CALL_MEMBER(linked_namespaces_P);
    linked.emplace_back(linked_namespace, shared_lib_sonames, allow_all_shared_libs);
  }
}

void android_namespace_t::add_linked_namespace(const android_namespace_link_t *link) {
  LinkerBlockLock lock;
  if (android_api <= __ANDROID_API_O_MR1__) {
    const auto *linkO = reinterpret_cast<const android_namespace_link_t_O *>(link);
    CALL_MEMBER(linked_namespaces_).push_back(*linkO);
  } else {
    const auto *linkP = reinterpret_cast<const android_namespace_link_t_P *>(link);
    CALL_MEMBER(linked_namespaces_P).push_back(*linkP);
  }
}

ANDROID_GE_R bool android_namespace_t::is_also_used_as_anonymous() { return CALL_MEMBER(is_also_used_as_anonymous_); }

ANDROID_GE_R void android_namespace_t::set_also_used_as_anonymous(bool yes) {
  LinkerBlockLock lock;
  CALL_MEMBER(is_also_used_as_anonymous_) = yes;
}

const std::vector<std::string> &android_namespace_t::get_ld_library_paths() { return CALL_MEMBER(ld_library_paths_); }

void android_namespace_t::set_ld_library_paths(std::vector<std::string> &&library_paths) {
  LinkerBlockLock lock;
  CALL_MEMBER(ld_library_paths_) = std::move(library_paths);
}

void android_namespace_t::add_ld_library_path(const std::string &path) {
  LinkerBlockLock lock;
  CALL_MEMBER(ld_library_paths_).push_back(path);
}

const std::vector<std::string> &android_namespace_t::get_default_library_paths() {
  return CALL_MEMBER(default_library_paths_);
}

void android_namespace_t::set_default_library_paths(std::vector<std::string> &&library_paths) {
  LinkerBlockLock lock;
  CALL_MEMBER(default_library_paths_) = std::move(library_paths);
}

void android_namespace_t::set_default_library_paths(const std::vector<std::string> &library_paths) {
  LinkerBlockLock lock;
  CALL_MEMBER(default_library_paths_) = library_paths;
}

void android_namespace_t::add_default_library_path(const std::string &path) {
  LinkerBlockLock lock;
  auto &libs = CALL_MEMBER(default_library_paths_);
  libs.push_back(path);
}

const std::vector<std::string> &android_namespace_t::get_permitted_paths() { return CALL_MEMBER(permitted_paths_); }

void android_namespace_t::set_permitted_paths(std::vector<std::string> &&permitted_paths) {
  LinkerBlockLock lock;
  CALL_MEMBER(permitted_paths_) = permitted_paths;
}

void android_namespace_t::set_permitted_paths(const std::vector<std::string> &permitted_paths) {
  LinkerBlockLock lock;
  CALL_MEMBER(permitted_paths_) = permitted_paths;
}

void android_namespace_t::add_permitted_path(const std::string &path) {
  LinkerBlockLock lock;
  CALL_MEMBER(permitted_paths_).push_back(path);
}

ANDROID_GE_Q std::vector<std::string> &android_namespace_t::get_whitelisted_libs() {
  return CALL_MEMBER(whitelisted_libs_);
}

ANDROID_GE_Q void android_namespace_t::set_whitelisted_libs(std::vector<std::string> &&whitelisted_libs) {
  LinkerBlockLock lock;
  CALL_MEMBER(whitelisted_libs_) = std::move(whitelisted_libs);
}

ANDROID_GE_Q void android_namespace_t::set_whitelisted_libs(const std::vector<std::string> &whitelisted_libs) {
  LinkerBlockLock lock;
  CALL_MEMBER(whitelisted_libs_) = whitelisted_libs;
}

ANDROID_GE_Q void android_namespace_t::add_whitelisted_lib(const std::string &lib) {
  LinkerBlockLock lock;
  CALL_MEMBER(whitelisted_libs_).push_back(lib);
}

ANDROID_GE_Q void android_namespace_t::remove_whitelisted_lib(const std::string &lib) {
  LinkerBlockLock lock;
  auto &libs = CALL_MEMBER(whitelisted_libs_);
  libs.erase(std::remove(libs.begin(), libs.end(), lib), libs.end());
}

void android_namespace_t::add_soinfo(const soinfo *si) {
  LinkerBlockLock lock;
  if (!find_soinfo(si)) {
    soinfo_list().push_back(const_cast<soinfo *>(si));
  }
}

void android_namespace_t::add_soinfos(const soinfo_list_t_wrapper soinfos) {
  LinkerBlockLock lock;
  for (auto si : soinfos) {
    if (!find_soinfo(si)) {
      add_soinfo(si);
    }
  }
}

bool android_namespace_t::find_soinfo(const soinfo *si) {
  LinkerBlockLock lock;
  soinfo *found;
  return soinfo_list().find_if([&](soinfo *find) {
    return find == si;
  }) != nullptr;
}

void android_namespace_t::remove_soinfo(const soinfo *si) {
  LinkerBlockLock lock;
  soinfo_list().remove_if([&](soinfo *candidate) {
    return si == candidate;
  });
}

soinfo_list_t_wrapper android_namespace_t::soinfo_list() {
  void *ref;
  if (android_api >= __ANDROID_API_T__) {
    ref = &CALL_MEMBER(soinfo_list_T);
  } else {
    ref = &CALL_MEMBER(soinfo_list_);
  }
  return soinfo_list_t_wrapper(ref);
}

bool android_namespace_t::is_accessible(const std::string &file) {
  if (!CALL_MEMBER(is_isolated_)) {
    return true;
  }
  if (android_api >= __ANDROID_API_Q__) {
    if (!CALL_MEMBER(whitelisted_libs_).empty()) {
      const char *lib_name = basename(file.c_str());
      if (std::find(CALL_MEMBER(whitelisted_libs_).begin(), CALL_MEMBER(whitelisted_libs_).end(), lib_name) ==
          CALL_MEMBER(whitelisted_libs_).end()) {
        return false;
      }
    }
  }
  for (const auto &dir : CALL_MEMBER(ld_library_paths_)) {
    if (file_is_in_dir(file, dir)) {
      return true;
    }
  }
  for (const auto &dir : CALL_MEMBER(default_library_paths_)) {
    if (file_is_in_dir(file, dir)) {
      return true;
    }
  }
  for (const auto &dir : CALL_MEMBER(permitted_paths_)) {
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
    if (si->get_primary_namespace() == this) {
      return true;
    }
    // When we're looking up symbols, we want to search libraries from the same
    // namespace (whether the namespace membership is primary or secondary), but
    // we also want to search the immediate dependencies of libraries in our
    // namespace. (e.g. Supposing that libapp.so -> libandroid.so crosses a
    // namespace boundary, we want to search libandroid.so but not any of
    // libandroid.so's dependencies).
    //
    // Some libraries may be present in this namespace via the secondary
    // namespace list:
    //  - the executable
    //  - LD_PRELOAD and DF_1_GLOBAL libraries
    //  - libraries inherited during dynamic namespace creation (e.g. because of
    //    RTLD_GLOBAL / DF_1_GLOBAL / ANDROID_NAMESPACE_TYPE_SHARED)
    //
    // When a library's membership is secondary, we want to search its symbols,
    // but not the symbols of its dependencies. The executable may depend on
    // internal system libraries which should not be searched.
    if (allow_secondary) {
      const android_namespace_list_t_wrapper secondary_namespaces = si->get_secondary_namespaces();
      if (secondary_namespaces.find(this) != secondary_namespaces.end()) {
        return true;
      }
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
    if ((si->dt_flags_1() & DF_1_GLOBAL) != 0) {
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
  if (this == fakelinker::ProxyLinker::GetDefaultNamespace()) {
    return get_global_group();
  }
  soinfo_list_t shared_group;
  soinfo_list().for_each([&](soinfo *si) {
    if ((si->get_rtld_flags() & RTLD_GLOBAL) != 0) {
      shared_group.push_back(si);
    }
  });
  return shared_group;
}

#undef AN_FUN
#undef CALL_MEMBER
#undef NP_MEMBER_REF_WRAP
#undef NP_MEMBER_REF_VER_WRAP
#undef NP_MEMBER_NULL