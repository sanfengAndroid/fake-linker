#pragma once

#include "linker_common_types.h"

#include <string>
#include <vector>
#include <unordered_set>

struct android_namespace_t;
#if __ANDROID_API__ >= __ANDROID_API_O__

struct android_namespace_link_t {
public:
	android_namespace_link_t(android_namespace_t *linked_namespace,
							 const std::unordered_set<std::string> &shared_lib_sonames,
							 bool allow_all_shared_libs)
			: linked_namespace_(linked_namespace), shared_lib_sonames_(shared_lib_sonames) {}

	android_namespace_t *linked_namespace() const {
		return linked_namespace_;
	}

	const std::unordered_set<std::string> &shared_lib_sonames() const {
		return shared_lib_sonames_;
	}

// 去掉常量限定,便于拷贝赋值
	android_namespace_t */*const*/ linked_namespace_;
	/*const*/ std::unordered_set<std::string> shared_lib_sonames_;
#if __ANDROID_API__ >= __ANDROID_API_P__
	bool allow_all_shared_libs_;
#endif
};

#endif

struct android_namespace_t {
public:
	android_namespace_t() :
			is_isolated_(false) {}

	const char *get_name() const {
#if __ANDROID_API__ >= __ANDROID_API_R__
		return name_.c_str();
#else
		return name_;
#endif
	}

	void set_name(const char *name) { name_ = name; }

	bool is_isolated() const { return is_isolated_; }

	void set_isolated(bool isolated) { is_isolated_ = isolated; }

#if __ANDROID_API__ >= __ANDROID_API_O__

	bool is_greylist_enabled() const { return is_greylist_enabled_; }

	void set_greylist_enabled(bool enabled) { is_greylist_enabled_ = enabled; }

	const std::vector<android_namespace_link_t> &linked_namespaces() const {
		return linked_namespaces_;
	}

	void add_linked_namespace(android_namespace_t *linked_namespace,
							  const std::unordered_set<std::string> &shared_lib_sonames,
							  bool allow_all_shared_libs) {
		linked_namespaces_.push_back(
				android_namespace_link_t(linked_namespace, shared_lib_sonames, allow_all_shared_libs));
	}

#endif
#if __ANDROID_API__ >= __ANDROID_API_R__

	bool is_also_used_as_anonymous() const { return is_also_used_as_anonymous_; }

	void set_also_used_as_anonymous(bool yes) { is_also_used_as_anonymous_ = yes; }

#endif

	const std::vector<std::string> &get_ld_library_paths() const {
		return ld_library_paths_;
	}

	void set_ld_library_paths(std::vector<std::string> &&library_paths) {
		ld_library_paths_ = std::move(library_paths);
	}

	const std::vector<std::string> &get_default_library_paths() const {
		return default_library_paths_;
	}

	void set_default_library_paths(std::vector<std::string> &&library_paths) {
		default_library_paths_ = std::move(library_paths);
	}

	void set_default_library_paths(const std::vector<std::string> &library_paths) {
		default_library_paths_ = library_paths;
	}

	const std::vector<std::string> &get_permitted_paths() const {
		return permitted_paths_;
	}

	void set_permitted_paths(std::vector<std::string> &&permitted_paths) {
		permitted_paths_ = std::move(permitted_paths);
	}

	void set_permitted_paths(const std::vector<std::string> &permitted_paths) {
		permitted_paths_ = permitted_paths;
	}

#if __ANDROID_API__ >= __ANDROID_API_Q__

	const std::vector<std::string> &get_whitelisted_libs() const {
		return whitelisted_libs_;
	}

	void set_whitelisted_libs(std::vector<std::string> &&whitelisted_libs) {
		whitelisted_libs_ = std::move(whitelisted_libs);
	}

	void set_whitelisted_libs(const std::vector<std::string> &whitelisted_libs) {
		whitelisted_libs_ = whitelisted_libs;
	}

	void add_whitelisted_lib(std::string lib) {
		whitelisted_libs_.push_back(lib);
	}

#endif

	void add_soinfo(soinfo *si) {
		if (!find_soinfo(si)) {
			soinfo_list_.push_back(si);
		}
	}

	void add_soinfos(const soinfo_list_t &soinfos) {
		for (auto si : soinfos) {
			if (!find_soinfo(si)) {
				add_soinfo(si);
			}
		}
	}

	bool find_soinfo(soinfo *si) {
		soinfo *found = soinfo_list_.find_if([&](soinfo *find) {
			return find == si;
		});
		return found != nullptr;
	}

	void remove_soinfo(soinfo *si) {
		soinfo_list_.remove_if([&](soinfo *candidate) {
			return si == candidate;
		});
	}

	const soinfo_list_t &soinfo_list() const { return soinfo_list_; }

	// For isolated namespaces - checks if the file is on the search path;
	// always returns true for not isolated namespace.
	bool is_accessible(const std::string &path);

	// Returns true if si is accessible from this namespace. A soinfo
	// is considered accessible when it belongs to this namespace
	// or one of it's parent soinfos belongs to this namespace.
	bool is_accessible(soinfo *si);

	soinfo_list_t get_global_group();

	soinfo_list_t get_shared_group();


	/*********************** 成员声明分割 **************************/
private:
#if __ANDROID_API__ >= __ANDROID_API_R__
	std::string name_;
#else
	const char *name_;
#endif
public:
	bool is_isolated_;

#if __ANDROID_API__ >= __ANDROID_API_O__
	bool is_greylist_enabled_;
#endif

#if __ANDROID_API__ >= __ANDROID_API_R__
	bool is_also_used_as_anonymous_;
#endif

	std::vector<std::string> ld_library_paths_;
	std::vector<std::string> default_library_paths_;
	std::vector<std::string> permitted_paths_;

#if __ANDROID_API__ >= __ANDROID_API_Q__
	std::vector<std::string> whitelisted_libs_;
#endif

	// Loader looks into linked namespace if it was not able
	// to find a library in this namespace. Note that library
	// lookup in linked namespaces are limited by the list of
	// shared sonames.
#if __ANDROID_API__ >= __ANDROID_API_O__
	std::vector<android_namespace_link_t> linked_namespaces_;
#endif

	soinfo_list_t soinfo_list_;

	DISALLOW_COPY_AND_ASSIGN(android_namespace_t);
};