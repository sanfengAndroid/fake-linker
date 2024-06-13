#pragma once

#include <link.h>
#include <stdint.h>

#include <linker_macros.h>

#include <map>

#include "linker_namespaces.h"
#include "local_block_allocator.h"

// https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linker_soinfo.h

#define SOINFO_NAME_LEN 128

#ifdef __LP64__
#define R_SYM  ELF64_R_SYM
#define R_TYPE ELF64_R_TYPE
#else
#define R_SYM  ELF32_R_SYM
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

struct SymbolLookupLib {
  uint32_t gnu_maskwords_ = 0;
  uint32_t gnu_shift2_ = 0;
  ElfW(Addr) *gnu_bloom_filter_ = nullptr;

  const char *strtab_;
  size_t strtab_size_;
  const ElfW(Sym) * symtab_;
  const ElfW(Versym) * versym_;

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

class SymbolName {
public:
  explicit SymbolName(const char *name) :
      name_(name), has_elf_hash_(false), has_gnu_hash_(false), elf_hash_(0), gnu_hash_(0) {}

  const char *get_name() { return name_; }

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

  bool init(soinfo *si_from);

  const version_info *get_version_info(ElfW(Versym) source_symver) const;

private:
  bool init_verneed(soinfo *si_from);

  bool init_verdef(soinfo *si_from);

  void add_version_info(size_t source_index, ElfW(Word) elf_hash, const char *ver_name, soinfo *target_si);

  std::vector<version_info> version_infos;

  DISALLOW_COPY_AND_ASSIGN(VersionTracker);
};

struct android_dlextinfo {
  /** A bitmask of `ANDROID_DLEXT_` enum values. */
  uint64_t flags;

  /** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and
   * `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
  void *reserved_addr;
  /** Used by `ANDROID_DLEXT_RESERVED_ADDRESS` and
   * `ANDROID_DLEXT_RESERVED_ADDRESS_HINT`. */
  size_t reserved_size;

  /** Used by `ANDROID_DLEXT_WRITE_RELRO` and `ANDROID_DLEXT_USE_RELRO`. */
  int relro_fd;

  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD`. */
  int library_fd;
  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET` */
};

ANDROID_GE_L1 struct android_dlextinfoL : android_dlextinfo {
  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET` */
  off64_t library_fd_offset;
};

ANDROID_GE_N struct android_dlextinfoN : android_dlextinfo {
  /** Used by `ANDROID_DLEXT_USE_LIBRARY_FD_OFFSET` */
  off64_t library_fd_offset;
  android_namespace_t *library_namespace;
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
  const void *init_ptr = ""; // Field is non-null even when init_size is 0.
  size_t init_size = 0;
};
struct soinfo_tls {
  TlsSegment segment;
  size_t module_id = 0;
};

typedef std::map<std::string, ElfW(Addr)> symbol_relocations;

struct soinfo {
  static void Init();

  const ElfW(Phdr) * phdr();

  size_t phnum();

  ElfW(Addr) base();

  size_t size();

  ElfW(Dyn) * dynamic();

  soinfo *next();

  uint32_t &flags();

  const char *strtab();

  ElfW(Sym) * symtab();

  size_t nbucket();

  size_t nchain();

  ANDROID_GE_M uint32_t *bucket_M();

  ANDROID_LE_L1 unsigned *bucket();

  ANDROID_GE_M uint32_t *chain_M();

  ANDROID_LE_L1 uint32_t *chain();

#ifndef __LP64__

  ElfW(Addr) * *plt_got();

#endif

#ifdef USE_RELA
  ElfW(Rela) * &plt_rela();
  size_t &plt_rela_count();

  ElfW(Rela) * &rela();
  size_t &rela_count();
#else

  ElfW(Rel) * &plt_rel();

  size_t &plt_rel_count();

  ElfW(Rel) * &rel();

  size_t &rel_count();

#endif

  ANDROID_LE_O1 linker_function_t *preinit_array();

  ANDROID_GE_P linker_ctor_function_t *preinit_array_P();

  size_t preinit_array_count();

  ANDROID_LE_O1 linker_function_t *init_array();

  ANDROID_GE_P linker_ctor_function_t *init_array_P();

  size_t init_array_count();

  ANDROID_LE_O1 linker_function_t *fini_array();

  ANDROID_GE_P linker_dtor_function_t *fini_array_P();

  size_t fini_array_count();

  ANDROID_LE_O1 linker_function_t init_func();

  ANDROID_GE_P linker_ctor_function_t init_func_P();

  ANDROID_LE_O1 linker_function_t fini_func();

  ANDROID_GE_P linker_dtor_function_t fini_func_P();

#ifdef __arm__
  uint32_t *ARM_exidx();
  size_t ARM_exidx_count();
#endif

  size_t ref_count();

  link_map &link_map_head();

  bool constructors_called();

  ElfW(Addr) load_bias();

#ifndef __LP64__

  bool has_text_relocations();

#endif

  bool has_DT_SYMBOLIC();

  uint32_t version();

  dev_t st_dev();

  ino_t st_ino();

  ANDROID_GE_L1 off64_t file_offset();

  ANDROID_GE_M uint32_t &rtld_flags();

  ANDROID_LE_L1 int &rtld_flags_L();

  ANDROID_GE_M uint32_t &dt_flags_1();

  ANDROID_GE_L1 size_t strtab_size();

  ANDROID_GE_M size_t gnu_nbucket();

  ANDROID_GE_M uint32_t *gnu_bucket();

  ANDROID_GE_M uint32_t *gnu_chain();

  ANDROID_GE_M uint32_t gnu_maskwords();

  ANDROID_GE_M uint32_t gnu_shift2();

  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter();

  ANDROID_GE_M soinfo *local_group_root();

  ANDROID_GE_M uint8_t *android_relocs();

  ANDROID_GE_M size_t android_relocs_size();

  ANDROID_GE_M const char *soname();

  ANDROID_GE_M const char *realpath();

  ANDROID_GE_M const ElfW(Versym) * versym();

  ANDROID_GE_M ElfW(Addr) verdef_ptr();

  ANDROID_GE_M size_t verdef_cnt();

  ANDROID_GE_M ElfW(Addr) verneed_ptr();

  ANDROID_GE_M size_t verneed_cnt();

  ANDROID_GE_M int target_sdk_version();

  ANDROID_GE_N std::vector<std::string> &dt_runpath();

  ANDROID_GE_N android_namespace_t *&primary_namespace();

  ANDROID_GE_N uintptr_t handle();

  ANDROID_GE_P ElfW(Relr) * relr();

  ANDROID_GE_P size_t relr_count();

  ANDROID_GE_Q std::unique_ptr<soinfo_tls> &tls();

  ANDROID_GE_Q std::vector<TlsDynamicResolverArg> &tlsdesc_args();

  ANDROID_GE_T ElfW(Addr) gap_start();

  ANDROID_GE_T size_t gap_size();

  /*****************************************************************/

  const char *get_soname();

  ElfW(Addr) resolve_symbol_address(const ElfW(Sym) * s);

  bool has_min_version(uint32_t min_version __unused);

  ANDROID_GE_M const ElfW(Versym) * get_versym_table();

  const char *get_string(ElfW(Word) index);

  void add_child(soinfo *child);

  soinfo_list_t_wrapper get_children();

  soinfo_list_t_wrapper get_parents();

  ANDROID_GE_L1 uint32_t get_rtld_flags();

  ANDROID_GE_M void set_dt_flags_1(uint32_t flag);

  bool is_linked();

  void set_linked();

  void set_unlinked();

  bool is_gnu_hash();

  /*
   * gnu hash只能查找导出符号, elf hash可以查找导入符号,
   * 虽然能查找到导入符号但是ElfW(Sym)中的值是0,因此屏蔽导入符号查找
   * */
  void *find_export_symbol_address(const char *name);

  void *find_import_symbol_address(const char *name);

  const ElfW(Sym) * find_export_symbol_by_name(SymbolName &symbol_name, const version_info *vi);

#ifdef USE_RELA

  const ElfW(Rela) * find_import_symbol_by_name(const char *name);

#else

  const ElfW(Rel) * find_import_symbol_by_name(const char *name);

#endif

  /*
   * gnu hash只能查找导出符号,导入符号没有Hash
   * */
  const ElfW(Sym) * gnu_lookup(SymbolName &symbol_name, const version_info *vi);

  /*
   * elf hash可以查找导入符号,但实际导入符号的数据都是空
   * */
  const ElfW(Sym) * elf_lookup(SymbolName &symbol_name, const version_info *vi);

  std::pair<uint32_t, uint32_t> get_export_symbol_gnu_table_size();

  size_t get_symbols_count();

  /*
   * 只获取全局符号,不获取弱符号,使用c++时可能存在很多弱符号,这些符号对重定向无意义
   * */
  symbol_relocations get_global_soinfo_export_symbols();

  symbol_relocations get_global_soinfo_export_symbols(const std::vector<std::string> &filters);

  bool again_process_relocation(symbol_relocations &rels);

  ANDROID_GE_M ElfW(Addr) get_verdef_ptr();

  ANDROID_GE_M size_t get_verdef_cnt();

  ANDROID_GE_R SymbolLookupLib get_lookup_lib();

  ANDROID_GE_N android_namespace_t *&get_primary_namespace();

  ANDROID_GE_N void add_secondary_namespace(android_namespace_t *secondary_ns);

  ANDROID_GE_N void remove_secondary_namespace(android_namespace_t *secondary_ns);

  ANDROID_GE_N void remove_all_secondary_namespace();

  ANDROID_GE_N android_namespace_list_t_wrapper get_secondary_namespaces();

  ANDROID_GE_N uintptr_t get_handle();

  static ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr);
};

// soinfo 所有成员访问都该经过方法调用,内部自动处理偏移问题
struct soinfoT : soinfo {
#ifdef __work_around_b_24465209__
  // old_name
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;

#ifdef __work_around_b_24465209__
  ElfW(Addr) unused0;
#endif

  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoT *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;
#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_ctor_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_ctor_function_t *init_array_;
  size_t init_array_count_;
  linker_dtor_function_t *fini_array_;
  size_t fini_array_count_;

  linker_ctor_function_t init_func_;
  linker_dtor_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t_T children_;
  soinfo_list_t_T parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoT *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M std::string soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M int target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t_T secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;

  // version >= 4
  ANDROID_GE_P ElfW(Relr) * relr_;
  ANDROID_GE_P size_t relr_count_;

  // version >= 5
  ANDROID_GE_Q std::unique_ptr<soinfo_tls> tls_;
  ANDROID_GE_Q std::vector<TlsDynamicResolverArg> tlsdesc_args_;

  // version >= 6
  ANDROID_GE_T ElfW(Addr) gap_start_;
  ANDROID_GE_T size_t gap_size_;
};

struct soinfoS : soinfo {
#ifdef __work_around_b_24465209__
  // old_name
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;

#ifdef __work_around_b_24465209__
  ElfW(Addr) unused0;
#endif

  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoS *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;
#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_ctor_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_ctor_function_t *init_array_;
  size_t init_array_count_;
  linker_dtor_function_t *fini_array_;
  size_t fini_array_count_;

  linker_ctor_function_t init_func_;
  linker_dtor_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  off64_t file_offset_;
  uint32_t rtld_flags_;
  uint32_t dt_flags_1_;
  size_t strtab_size_;

  // version >= 2
  size_t gnu_nbucket_;
  uint32_t *gnu_bucket_;
  uint32_t *gnu_chain_;
  uint32_t gnu_maskwords_;
  uint32_t gnu_shift2_;
  ElfW(Addr) * gnu_bloom_filter_;
  soinfoS *local_group_root_;
  uint8_t *android_relocs_;
  size_t android_relocs_size_;
  std::string soname_;
  std::string realpath_;
  const ElfW(Versym) * versym_;
  ElfW(Addr) verdef_ptr_;
  size_t verdef_cnt_;

  ElfW(Addr) verneed_ptr_;
  size_t verneed_cnt_;
  int target_sdk_version_;

  // version >= 3
  std::vector<std::string> dt_runpath_;
  android_namespace_t *primary_namespace_;
  android_namespace_list_t secondary_namespaces_;
  uintptr_t handle_;

  // version >= 4
  ElfW(Relr) * relr_;
  size_t relr_count_;

  // version >= 5
  std::unique_ptr<soinfo_tls> tls_;
  std::vector<TlsDynamicResolverArg> tlsdesc_args_;

  // version >= 6
  ElfW(Addr) gap_start_;
  size_t gap_size_;
};

struct soinfoQ : soinfo {
#ifdef __work_around_b_24465209__
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;

#ifdef __work_around_b_24465209__
  ElfW(Addr) unused0;
#endif

  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoQ *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_ctor_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_ctor_function_t *init_array_;
  size_t init_array_count_;
  linker_dtor_function_t *fini_array_;
  size_t fini_array_count_;

  linker_ctor_function_t init_func_;
  linker_dtor_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoQ *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M int target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;

  // version >= 4
  ANDROID_GE_P ElfW(Relr) * relr_;
  ANDROID_GE_P size_t relr_count_;

  // version >= 5
  ANDROID_GE_Q std::unique_ptr<soinfo_tls> tls_;
  ANDROID_GE_Q std::vector<TlsDynamicResolverArg> tlsdesc_args_;
};
using soinfoR = soinfoQ;

struct soinfoP : soinfo {
#if defined(__work_around_b_24465209__)
  char name_[SOINFO_NAME_LEN];
#endif
  const ElfW(Phdr) * phdr;
  size_t phnum;

#if defined(__work_around_b_24465209__)
  ElfW(Addr) unused0; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif
  soinfoP *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_ctor_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_ctor_function_t *init_array_;
  size_t init_array_count_;
  linker_dtor_function_t *fini_array_;
  size_t fini_array_count_;

  linker_ctor_function_t init_func_;
  linker_dtor_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoP *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M uint32_t target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;

  // version >= 4
  ANDROID_GE_P ElfW(Relr) * relr_;
  ANDROID_GE_P size_t relr_count_;
};

struct soinfoO : soinfo {
#if defined(__work_around_b_24465209__)
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;
#if defined(__work_around_b_24465209__)
  ElfW(Addr) unused0;
#endif
  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoO *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoO *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M uint32_t target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;
};
struct soinfoN1 : soinfo {
#if defined(__work_around_b_24465209__)
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoN1 *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoN1 *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M uint32_t target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;
};

struct soinfoN : soinfo {
#if defined(__work_around_b_24465209__)
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoN *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;
  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoN *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M uint32_t target_sdk_version_;

  // version >= 3
  ANDROID_GE_N std::vector<std::string> dt_runpath_;
  ANDROID_GE_N android_namespace_t *primary_namespace_;
  ANDROID_GE_N android_namespace_list_t secondary_namespaces_;
  ANDROID_GE_N uintptr_t handle_;
};

struct soinfoM : soinfo {
#if defined(__work_around_b_24465209__)
  char name_[SOINFO_NAME_LEN];
#endif

  const ElfW(Phdr) * phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#if defined(__work_around_b_24465209__)
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#if defined(__work_around_b_24465209__)
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoM *next;
  uint32_t flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  uint32_t *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;

  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 uint32_t rtld_flags_;
  ANDROID_GE_M uint32_t dt_flags_1_;
  ANDROID_GE_L1 size_t strtab_size_;

  // version >= 2
  ANDROID_GE_M size_t gnu_nbucket_;
  ANDROID_GE_M uint32_t *gnu_bucket_;
  ANDROID_GE_M uint32_t *gnu_chain_;
  ANDROID_GE_M uint32_t gnu_maskwords_;
  ANDROID_GE_M uint32_t gnu_shift2_;
  ANDROID_GE_M ElfW(Addr) * gnu_bloom_filter_;
  ANDROID_GE_M soinfoM *local_group_root_;
  ANDROID_GE_M uint8_t *android_relocs_;
  ANDROID_GE_M size_t android_relocs_size_;
  ANDROID_GE_M const char *soname_;
  ANDROID_GE_M std::string realpath_;
  ANDROID_GE_M const ElfW(Versym) * versym_;
  ANDROID_GE_M ElfW(Addr) verdef_ptr_;
  ANDROID_GE_M size_t verdef_cnt_;
  ANDROID_GE_M ElfW(Addr) verneed_ptr_;
  ANDROID_GE_M size_t verneed_cnt_;
  ANDROID_GE_M uint32_t target_sdk_version_;
};

struct soinfoL1 : soinfo {
  char name_[SOINFO_NAME_LEN];
  const ElfW(Phdr) * phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#ifndef __LP64__
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#ifndef __LP64__
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoL1 *next;
  unsigned flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  unsigned *bucket_;
  unsigned *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  unsigned *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;

  uint32_t version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;

  // version >= 1
  ANDROID_GE_L1 off64_t file_offset_;
  ANDROID_GE_L1 int rtld_flags_;
  ANDROID_GE_L1 size_t strtab_size_;
};

struct soinfoL : soinfo {
  char name_[SOINFO_NAME_LEN];
  const ElfW(Phdr) * phdr;
  size_t phnum;
  ElfW(Addr) entry;
  ElfW(Addr) base;
  size_t size;

#ifndef __LP64__
  uint32_t unused1; // DO NOT USE, maintained for compatibility.
#endif

  ElfW(Dyn) * dynamic;

#ifndef __LP64__
  uint32_t unused2; // DO NOT USE, maintained for compatibility
  uint32_t unused3; // DO NOT USE, maintained for compatibility
#endif

  soinfoL *next;
  unsigned flags_;
  const char *strtab_;
  ElfW(Sym) * symtab_;
  size_t nbucket_;
  size_t nchain_;
  unsigned *bucket_;
  unsigned *chain_;

#ifndef __LP64__
  ElfW(Addr) * *plt_got_;
#endif

#ifdef USE_RELA
  ElfW(Rela) * plt_rela_;
  size_t plt_rela_count_;

  ElfW(Rela) * rela_;
  size_t rela_count_;
#else
  ElfW(Rel) * plt_rel_;
  size_t plt_rel_count_;

  ElfW(Rel) * rel_;
  size_t rel_count_;
#endif

  linker_function_t *preinit_array_;
  size_t preinit_array_count_;

  linker_function_t *init_array_;
  size_t init_array_count_;
  linker_function_t *fini_array_;
  size_t fini_array_count_;

  linker_function_t init_func_;
  linker_function_t fini_func_;

#ifdef __arm__
  unsigned *ARM_exidx;
  size_t ARM_exidx_count;
#endif

  size_t ref_count_;
  link_map link_map_head;
  bool constructors_called;
  ElfW(Addr) load_bias;

#ifndef __LP64__
  bool has_text_relocations;
#endif

  bool has_DT_SYMBOLIC;

  unsigned int version_;

  // version >= 0
  dev_t st_dev_;
  ino_t st_ino_;
  soinfo_list_t children_;
  soinfo_list_t parents_;
};

const char *fix_dt_needed(const char *dt_needed, const char *sopath);

template <typename F>
void for_each_dt_needed(soinfo *si, F action) {
  for (const ElfW(Dyn) *d = si->dynamic(); d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_NEEDED) {
      action(fix_dt_needed(si->get_string(d->d_un.d_val), si->realpath()));
    }
  }
}
