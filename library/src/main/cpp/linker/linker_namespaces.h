#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <linker_macros.h>

#include "linker_common_types.h"

// https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linker_namespaces.h

struct android_namespace_t;

ANDROID_GE_O struct android_namespace_link_t {
public:
  ANDROID_GE_O android_namespace_link_t(android_namespace_t *linked_namespace,
                                        const std::unordered_set<std::string> &shared_lib_sonames);

  ANDROID_GE_O android_namespace_t *linked_namespace() const { return linked_namespace_; }

  ANDROID_GE_O const std::unordered_set<std::string> &shared_lib_sonames() const { return shared_lib_sonames_; }

  // 去掉常量限定,便于拷贝赋值
  android_namespace_t * /*const*/ linked_namespace_;
  /*const*/ std::unordered_set<std::string> shared_lib_sonames_;
};

ANDROID_GE_P struct android_namespace_link_t_P : android_namespace_link_t {
  ANDROID_GE_P bool allow_all_shared_libs_;

  android_namespace_link_t_P(android_namespace_t *linked_namespace,
                             const std::unordered_set<std::string> &shared_lib_sonames, bool allow_all_shared_libs) :
      android_namespace_link_t(linked_namespace, shared_lib_sonames), allow_all_shared_libs_(allow_all_shared_libs) {}
};

ANDROID_GE_O struct android_namespace_link_t_O : android_namespace_link_t {
  android_namespace_link_t_O(android_namespace_t *linked_namespace,
                             const std::unordered_set<std::string> &shared_lib_sonames) :
      android_namespace_link_t(linked_namespace, shared_lib_sonames) {}
};

ANDROID_GE_N struct android_namespace_t {
public:
  android_namespace_t();
  static void Init();

  const char *get_name();

  void set_name(const char *name);

  bool is_isolated();

  void set_isolated(bool isolated);

  ANDROID_GE_O bool is_greylist_enabled();

  ANDROID_GE_O void set_greylist_enabled(bool enabled);

  ANDROID_GE_P const std::vector<android_namespace_link_t_P> &linked_namespaceP();

  ANDROID_LE_O1 ANDROID_GE_O const std::vector<android_namespace_link_t_O> &linked_namespacesO();

  ANDROID_GE_O void add_linked_namespace(android_namespace_t *linked_namespace,
                                         const std::unordered_set<std::string> &shared_lib_sonames,
                                         bool allow_all_shared_libs);

  ANDROID_GE_O void add_linked_namespace(const android_namespace_link_t *link);

  ANDROID_GE_R bool is_also_used_as_anonymous();

  ANDROID_GE_R void set_also_used_as_anonymous(bool yes);

  const std::vector<std::string> &get_ld_library_paths();

  void set_ld_library_paths(std::vector<std::string> &&library_paths);

  void add_ld_library_path(const std::string &path);

  const std::vector<std::string> &get_default_library_paths();

  void set_default_library_paths(std::vector<std::string> &&library_paths);

  void set_default_library_paths(const std::vector<std::string> &library_paths);

  void add_default_library_path(const std::string &path);

  const std::vector<std::string> &get_permitted_paths();

  void set_permitted_paths(std::vector<std::string> &&permitted_paths);

  void set_permitted_paths(const std::vector<std::string> &permitted_paths);

  void add_permitted_path(const std::string &path);

  ANDROID_GE_Q std::vector<std::string> &get_whitelisted_libs();

  ANDROID_GE_Q void set_whitelisted_libs(std::vector<std::string> &&whitelisted_libs);
  ANDROID_GE_Q void set_whitelisted_libs(const std::vector<std::string> &whitelisted_libs);

  ANDROID_GE_Q void add_whitelisted_lib(const std::string &lib);

  ANDROID_GE_Q void remove_whitelisted_lib(const std::string &lib);

  void add_soinfo(const soinfo *si);

  void add_soinfos(const soinfo_list_t_wrapper soinfos);

  bool find_soinfo(const soinfo *si);

  void remove_soinfo(const soinfo *si);

  soinfo_list_t_wrapper soinfo_list();

  // For isolated namespaces - checks if the file is on the search path;
  // always returns true for not isolated namespace.
  bool is_accessible(const std::string &path);

  // Returns true if si is accessible from this namespace. A soinfo
  // is considered accessible when it belongs to this namespace
  // or one of it's parent soinfos belongs to this namespace.
  bool is_accessible(soinfo *si);

  soinfo_list_t get_global_group();

  soinfo_list_t get_shared_group();
};

struct android_namespace_t_T : android_namespace_t {
  std::string name_;
  bool is_isolated_;
  // android 12 is_exempt_list_enabled_
  bool is_greylist_enabled_;
  bool is_also_used_as_anonymous_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  // android 12 allowed_libs_
  std::vector<std::string> whitelisted_libs_;
  std::vector<android_namespace_link_t_P> linked_namespaces_;
  soinfo_list_t_T soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_T);
};

struct android_namespace_t_R : android_namespace_t {
  std::string name_;
  bool is_isolated_;
  // android 12 is_exempt_list_enabled_
  bool is_greylist_enabled_;
  bool is_also_used_as_anonymous_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  // android 12 allowed_libs_
  std::vector<std::string> whitelisted_libs_;
  std::vector<android_namespace_link_t_P> linked_namespaces_;
  soinfo_list_t soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_R);
};

struct android_namespace_t_Q : android_namespace_t {
  const char *name_;
  bool is_isolated_;
  bool is_greylist_enabled_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  std::vector<std::string> whitelisted_libs_;
  std::vector<android_namespace_link_t_P> linked_namespaces_;
  soinfo_list_t soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_Q);
};

struct android_namespace_t_P : android_namespace_t {
  const char *name_;
  bool is_isolated_;
  bool is_greylist_enabled_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  std::vector<android_namespace_link_t_P> linked_namespaces_;
  soinfo_list_t soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_P);
};

struct android_namespace_t_O : android_namespace_t {
  const char *name_;
  bool is_isolated_;
  bool is_greylist_enabled_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  std::vector<android_namespace_link_t_O> linked_namespaces_;
  soinfo_list_t soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_O);
};

struct android_namespace_t_N : android_namespace_t {
  const char *name_;
  bool is_isolated_;

  std::vector<std::string> ld_library_paths_;
  std::vector<std::string> default_library_paths_;
  std::vector<std::string> permitted_paths_;
  soinfo_list_t soinfo_list_;
  DISALLOW_COPY_AND_ASSIGN(android_namespace_t_N);
};