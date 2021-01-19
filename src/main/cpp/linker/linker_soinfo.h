#pragma once

#include <link.h>

#include <variable_length_object.h>

#include "linker_namespaces.h"
#include "local_block_allocator.h"

#define SOINFO_NAME_LEN 128

#ifdef __LP64__
#define R_SYM ELF64_R_SYM
#define R_TYPE ELF64_R_TYPE
#else
#define R_SYM ELF32_R_SYM
#define R_TYPE ELF32_R_TYPE

#endif

#if defined(USE_RELA)
typedef ElfW(Rela) rel_t;
#else
typedef ElfW(Rel) rel_t;
#endif

#define FLAG_LINKED           0x00000001
#define FLAG_EXE              0x00000004 // The main executable
#define FLAG_LINKER           0x00000010 // The linker itself
#define FLAG_GNU_HASH         0x00000040 // uses gnu hash
#define FLAG_MAPPED_BY_CALLER 0x00000080 // the map is reserved by the caller
#define FLAG_IMAGE_LINKED     0x00000100 // Is image linked - this is a guard on link_image.
#define FLAG_RESERVED         0x00000200 // This flag was set when there is at least one
#define FLAG_PRELINKED        0x00000400 // prelink_image has successfully processed this soinfo
#define FLAG_NEW_SOINFO       0x40000000 // new soinfo format


ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr);

typedef void (*linker_dtor_function_t)();

typedef void (*linker_ctor_function_t)(int, char **, char **);

typedef void (*linker_function_t)();

// An entry within a SymbolLookupList.
#if __ANDROID_API__ >= __ANDROID_API_R__
struct SymbolLookupLib {
	uint32_t gnu_maskwords_ = 0;
	uint32_t gnu_shift2_ = 0;
	ElfW(Addr) *gnu_bloom_filter_ = nullptr;

	const char *strtab_;
	size_t strtab_size_;
	const ElfW(Sym) *symtab_;
	const ElfW(Versym) *versym_;

	const uint32_t *gnu_chain_;
	size_t gnu_nbucket_;
	uint32_t *gnu_bucket_;

	soinfo *si_ = nullptr;

	bool needs_sysv_lookup() const { return si_ != nullptr && gnu_bloom_filter_ == nullptr; }
};

// A list of libraries to search for a symbol.
class SymbolLookupList {
	std::vector<SymbolLookupLib> libs_;
	SymbolLookupLib sole_lib_;
	const SymbolLookupLib *begin_;
	const SymbolLookupLib *end_;
	size_t slow_path_count_ = 0;

public:
	explicit SymbolLookupList(soinfo *si);

	SymbolLookupList(const soinfo_list_t &global_group, const soinfo_list_t &local_group);

	void set_dt_symbolic_lib(soinfo *symbolic_lib);

	const SymbolLookupLib *begin() const { return begin_; }

	const SymbolLookupLib *end() const { return end_; }

	bool needs_slow_path() const { return slow_path_count_ > 0; }
};
#endif
typedef struct {
	/** A bitmask of `ANDROID_DLEXT_` enum values. */
	uint64_t flags;

	/** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
	void *reserved_addr;
	/** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
	size_t reserved_size;

	/** Used by `ANDROID_DLEXT_WRITE_RELRO` and `ANDROID_DLEXT_USE_RELRO`. */
	int relro_fd;

	/** Used by `ANDROID_DLEXT_USE_LIBRARY_FD`. */
	int library_fd;
	/** Used by `ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET` */
#if __ANDROID_API__ > __ANDROID_API_L__
	off64_t library_fd_offset;
#endif

	/** Used by `ANDROID_DLEXT_USE_NAMESPACE`. */
#if __ANDROID_API__ >= __ANDROID_API_N__
	struct android_namespace_t *library_namespace;
#endif
} android_dlextinfo;

class SymbolName {
public:
	explicit SymbolName(const char *name)
			: name_(name), has_elf_hash_(false), has_gnu_hash_(false),
			  elf_hash_(0), gnu_hash_(0) {}

	const char *get_name() {
		return name_;
	}

	uint32_t elf_hash();

	uint32_t gnu_hash();

private:
	const char *name_;
	bool has_elf_hash_;
	bool has_gnu_hash_;
	uint32_t elf_hash_;
	uint32_t gnu_hash_;
};

struct version_info {
	constexpr version_info() : elf_hash(0), name(nullptr), target_si(nullptr) {}

	uint32_t elf_hash;
	const char *name;
	const soinfo *target_si;
};

class VersionTracker {
public:
	VersionTracker() = default;

	bool init(const soinfo *si_from);

	const version_info *get_version_info(ElfW(Versym) source_symver) const;

private:
	bool init_verneed(const soinfo *si_from);

	bool init_verdef(const soinfo *si_from);

	void add_version_info(size_t source_index, ElfW(Word) elf_hash,
						  const char *ver_name, const soinfo *target_si);

	std::vector<version_info> version_infos;

	DISALLOW_COPY_AND_ASSIGN(VersionTracker);
};

struct TlsIndex {
	size_t module_id;
	size_t offset;
};

struct TlsDynamicResolverArg {
	size_t generation;
	TlsIndex index;
};
struct TlsSegment {
	size_t size = 0;
	size_t alignment = 1;
	const void *init_ptr = "";    // Field is non-null even when init_size is 0.
	size_t init_size = 0;
};
struct soinfo_tls {
	TlsSegment segment;
	size_t module_id = 0;
};

struct symbol_relocation {
	const char *name;
	/*
	 * resolve_symbol_address 解析后的值
	 * */
	ElfW(Addr) sym_address;
	/*
	 * 调试时使用
	 * */
	soinfo *source;
};

//typedef struct {
//	size_t len;
//	symbol_relocation symbols[0];
//} symbol_relocations;

typedef VarLengthObject<symbol_relocation> symbol_relocations;

struct soinfo {
public:
	// 不能构造新对象
	soinfo(android_namespace_t *ns, const char *name, const struct stat *file_stat,
		   off64_t file_offset, int rtld_flags);

	~soinfo();

	ElfW(Addr) resolve_symbol_address(const ElfW(Sym) *s) const;

	bool inline has_min_version(uint32_t min_version __unused) const {
#if defined(__work_around_b_24465209__)
		return (flags_ & FLAG_NEW_SOINFO) != 0 && version_ >= min_version;
#else
		return true;
#endif
	}

	const ElfW(Versym) *get_versym_table() const;

	const char *get_realpath() const;

	const char *get_soname() const;

	const char *get_string(ElfW(Word) index) const;

	void add_child(soinfo *child);

	const soinfo_list_t &get_children() const;

	soinfo_list_t &get_parents();

#if __ANDROID_API__ >= __ANDROID_API_L_MR1__

	uint32_t get_rtld_flags() const;

#endif

	uint32_t get_dt_flags_1() const;

#if __ANDROID_API__ >= __ANDROID_API_M__

	void set_dt_flags_1(uint32_t dt_flags_1);

#endif

	bool is_linked() const;

	void set_linked();

	void set_unlinked();

	bool is_gnu_hash() const;

	/*
	 * gnu hash只能查找导出符号
	 * elf hash可以查找导入符号,虽然能查找到导入符号但是ElfW(Sym)中的值是0,因此屏蔽导入符号查找
	 * */
	void *find_export_symbol_address(const char *name) const;

	void *find_import_symbol_address(const char *name) const;

	ElfW(Addr) get_verdef_ptr() const;

	size_t get_verdef_cnt() const;


	const ElfW(Sym) *find_export_symbol_by_name(SymbolName &symbol_name, const version_info *vi) const;

	/*
	 * gnu hash只能查找导出符号,导入符号没有Hash
	 * */
	const ElfW(Sym) *gnu_lookup(SymbolName &symbol_name, const version_info *vi) const;

	/*
	 * elf hash可以查找导入符号,但实际导入符号的数据都是空
	 * */
	const ElfW(Sym) *elf_lookup(SymbolName &symbol_name, const version_info *vi) const;

	size_t get_symbols_count();

	/*
	 * 只获取全局符号,不获取弱符号,使用c++时可能存在很多弱符号,这些符号对重定向无意义
	 * */
	symbol_relocations *get_global_soinfo_export_symbols();

	symbol_relocations *get_global_soinfo_export_symbols(std::vector<std::string>& filters);

#ifdef USE_RELA

	const ElfW(Rela) *find_import_symbol_by_name(const char *name) const;

#else

	const ElfW(Rel) *find_import_symbol_by_name(const char *name) const;

#endif

	std::pair<uint32_t, uint32_t> get_export_symbol_gnu_table_size();

	/*
	 * 手动解析重定位符号并修正
	 * */
	bool again_process_relocation(symbol_relocations *rels);

#if __ANDROID_API__ >= __ANDROID_API_R__
	SymbolLookupLib get_lookup_lib();
#endif

#if __ANDROID_API__ >= __ANDROID_API_N__

	android_namespace_t *get_primary_namespace();

	void add_secondary_namespace(android_namespace_t *secondary_ns);

	android_namespace_list_t &get_secondary_namespaces();

	uintptr_t get_handle() const;

#endif
	/****************** 定义分割 **********************/
public:

#if __ANDROID_API__ <= __ANDROID_API_L_MR1__
	char name_[SOINFO_NAME_LEN];
#elif defined(__work_around_b_24465209__)
	char old_name_[SOINFO_NAME_LEN];
#endif

	const ElfW(Phdr) *phdr;
	size_t phnum;

#if __ANDROID_API__ <= __ANDROID_API_N_MR1__
	ElfW(Addr) entry;
#elif defined(__work_around_b_24465209__)
	ElfW(Addr) unused0; // DO NOT USE, maintained for compatibility.
#endif

	ElfW(Addr) base;
	size_t size;

#if defined(__work_around_b_24465209__)
	uint32_t unused1;  // DO NOT USE, maintained for compatibility.
#endif

	ElfW(Dyn) *dynamic;

#if defined(__work_around_b_24465209__)
	uint32_t unused2; // DO NOT USE, maintained for compatibility
	uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif
public:
	soinfo *next;
//private:
#if __ANDROID_API__ <= __ANDROID_API_L_MR1__
	unsigned flags_;
#else
	uint32_t flags_;
#endif

	const char *strtab_;
	ElfW(Sym) *symtab_;

	size_t nbucket_;
	size_t nchain_;
#if __ANDROID_API__ <= __ANDROID_API_L_MR1__
	unsigned *bucket_;
	unsigned *chain_;
#else
	uint32_t *bucket_;
	uint32_t *chain_;
#endif

#if !defined(__LP64__)
#if __ANDROID_API__ >= __ANDROID_API_R__
	ElfW(Addr) **unused4; // DO NOT USE, maintained for compatibility
#else
	ElfW(Addr) **plt_got_;
#endif
#endif

#if defined(USE_RELA)
	ElfW(Rela) *plt_rela_;
	size_t plt_rela_count_;

	ElfW(Rela) *rela_;
	size_t rela_count_;
#else
	ElfW(Rel) *plt_rel_;
	size_t plt_rel_count_;

	ElfW(Rel) *rel_;
	size_t rel_count_;
#endif

#if __ANDROID_API__ <= __ANDROID_API_N_MR1__
	linker_function_t *preinit_array_;
	size_t preinit_array_count_;

	linker_function_t *init_array_;
	size_t init_array_count_;
	linker_function_t *fini_array_;
	size_t fini_array_count_;

	linker_function_t init_func_;
	linker_function_t fini_func_;
#else
	linker_ctor_function_t *preinit_array_;
	size_t preinit_array_count_;

	linker_ctor_function_t *init_array_;
	size_t init_array_count_;
	linker_dtor_function_t *fini_array_;
	size_t fini_array_count_;

	linker_ctor_function_t init_func_;
	linker_dtor_function_t fini_func_;
#endif

#if defined(__arm__)
	public:
	  // ARM EABI section used for stack unwinding.
#if __ANDROID_API__ <= __ANDROID_API_L_MR1__
	unsigned *ARM_exidx;
#else
	uint32_t* ARM_exidx;
#endif

	size_t ARM_exidx_count;
#endif

	size_t ref_count_;

	link_map link_map_head;

	bool constructors_called;

	ElfW(Addr) load_bias;

#if !defined(__LP64__)
	bool has_text_relocations;
#endif

	bool has_DT_SYMBOLIC;

public:
#if __ANDROID_API__ <= __ANDROID_API_L__
	unsigned int version_;
#else
	uint32_t version_;
#endif

	// version >= 0
	dev_t st_dev_;
	ino_t st_ino_;

	// dependency graph
	soinfo_list_t children_;
	soinfo_list_t parents_;

#if __ANDROID_API__ >= __ANDROID_API_L_MR1__        // 5.1以上
	// version >= 1
	off64_t file_offset_;

#if __ANDROID_API__ == __ANDROID_API_L_MR1__
	int rtld_flags_;
#else
	uint32_t rtld_flags_;
#endif

#if __ANDROID_API__ >= __ANDROID_API_M__        // 6.0以上
	uint32_t dt_flags_1_;
#endif

	size_t strtab_size_;
#endif

#if __ANDROID_API__ >= __ANDROID_API_M__        // 6.0以上
	// version >= 2

	size_t gnu_nbucket_;
	/*
	 * 导出符号在符号表中的起始索引
	 * */
	uint32_t *gnu_bucket_;
	uint32_t *gnu_chain_;
	uint32_t gnu_maskwords_;
	uint32_t gnu_shift2_;
	ElfW(Addr) *gnu_bloom_filter_;

	soinfo *local_group_root_;

	uint8_t *android_relocs_;
	size_t android_relocs_size_;

	const char *soname_;
	std::string realpath_;

	const ElfW(Versym) *versym_;

	ElfW(Addr) verdef_ptr_;
	size_t verdef_cnt_;

	ElfW(Addr) verneed_ptr_;
	size_t verneed_cnt_;
#if __ANDROID_API__ >= __ANDROID_API_Q__
	int target_sdk_version_;
#else
	uint32_t target_sdk_version_;
#endif
#endif

#if __ANDROID_API__ >= __ANDROID_API_N__        // 7.0以上
	// version >= 3
	std::vector<std::string> dt_runpath_;
	android_namespace_t *primary_namespace_;
	android_namespace_list_t secondary_namespaces_;
	uintptr_t handle_;
#endif

#if __ANDROID_API__ >= __ANDROID_API_P__        // 9.0以上
	// version >= 4
	ElfW(Relr) *relr_;
	size_t relr_count_;
#endif

#if __ANDROID_API__ >= __ANDROID_API_Q__        // 10.0以上
	// version >= 5
	std::unique_ptr<soinfo_tls> tls_;
	std::vector<TlsDynamicResolverArg> tlsdesc_args_;
#endif
};

// This function is used by dlvsym() to calculate hash of sym_ver


const char *fix_dt_needed(const char *dt_needed, const char *sopath);

template<typename F>
void for_each_dt_needed(const soinfo *si, F action) {
	for (const ElfW(Dyn) *d = si->dynamic; d->d_tag != DT_NULL; ++d) {
		if (d->d_tag == DT_NEEDED) {
			action(fix_dt_needed(si->get_string(d->d_un.d_val), si->get_realpath()));
		}
	}
}
