#pragma once

#include <list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "unique_fd.h"
#include "unique_memory.h"

#include "linker.h"
#include "linker_mapped_file_fragment.h"
#include "linker_note_gnu_property.h"

#ifdef USE_RELA
#define ELF_REL ElfW(Rela)
#else
#define ELF_REL ElfW(Rel)
#endif


namespace fakelinker {
class MapsHelper;

struct ElfDiskInfo {
  Address section_strtab_offset;
  Address section_strtab_addr = 0; // Actual address in memory
  uintptr_t section_strtab_size;   // Section size
  uintptr_t str_addralign;

  Address section_symtab_offset;
  Address section_symtab_addr = 0;
  uintptr_t section_symtab_size;
  uintptr_t sym_entsize;
  uintptr_t sym_addralign;
  uintptr_t sym_num;

  Address section_debugdata_addr = 0;
  uintptr_t section_debugdata_size;
  std::string debugdata;

  unique_memory mmap_memory;
  unique_fd library_fd;
  Address base;

  std::map<std::string_view, const ElfW(Sym) *> internal_symbols;
};

#define MAYBE_MAP_FLAG(x, from, to) (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)                                                                                              \
  (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

class ElfReader {
public:
  ElfReader();

  bool Read(const char *name, int fd, off64_t file_offset, off64_t file_size);
  bool ReadFormDisk(int fd, off64_t file_size);
  bool Load(address_space_params *address_space);
  bool LoadFromMemory(const char *name);
  bool LoadFromDisk(const char *library_name);
  // Cache internal symbols to accelerate lookup
  bool CacheInternalSymbols();
  bool DecompressDebugData();

  const char *name() const { return name_.c_str(); }

  size_t phdr_count() const { return phdr_num_; }

  ElfW(Addr) load_start() const { return reinterpret_cast<ElfW(Addr)>(load_start_); }

  size_t load_size() const { return load_size_; }

  ElfW(Addr) gap_start() const { return reinterpret_cast<ElfW(Addr)>(gap_start_); }

  size_t gap_size() const { return gap_size_; }

  ElfW(Addr) load_bias() const { return load_bias_; }

  const ElfW(Phdr) * loaded_phdr() const { return loaded_phdr_; }

  const ElfW(Dyn) * dynamic() const { return dynamic_; }

  const char *get_string(ElfW(Word) index) const;

  bool is_mapped_by_caller() const { return mapped_by_caller_; }

  ElfW(Addr) entry_point() const { return header_.e_entry + load_bias_; }

  const ElfW(Sym) * GnuHashLookupSymbol(const char *name);
  const ElfW(Sym) * GnuImportLookupSymbol(const char *name);
  const ElfW(Sym) * ElfHashLookupSymbol(const char *name);

  uint64_t FindImportSymbol(const char *name);
  std::vector<Address> FindImportSymbols(const std::vector<std::string> &symbols);

  uint64_t FindExportSymbol(const char *name);
  std::vector<Address> FindExportSymbols(const std::vector<std::string> &symbols);

  bool IterateInternalSymbols(const std::function<bool(std::string_view, const ElfW(Sym) *)> &callback);
  uint64_t FindInternalSymbol(std::string_view name, bool useRegex = false);
  uint64_t FindInternalSymbolByPrefix(std::string_view prefix);

  /**
   * Find internal symbol addresses, supports regular expressions. Regular expressions use default std::regex
   * so callers must ensure regex expressions are valid.
   * Note: Although regex is supported, symbol names are matched first for convenience of exact symbol
   * matching and regex matching together, avoiding multiple traversals of the symbol table.
   *
   * @param symbols  Collection of internal symbol names or regex expressions to find, ensure non-empty to avoid
   * searching entire symbol table
   * @param useRegex Enable regex matching support
   * @return         Return address collection, found addresses are non-zero
   */
  std::vector<Address> FindInternalSymbols(const std::vector<std::string> &symbols, bool useRegex = false);

private:
  bool ReadElfHeader();
  bool VerifyElfHeader();
  bool ReadProgramHeaders(bool map_memory = true);
  bool ReadProgramHeadersFromMemory(const char *base, MapsHelper &maps);
  bool ReadSectionHeaders(bool map_memory = false);
  bool ReadSectionHeadersFromMemory();
  bool ReadDynamicSection();
  bool ReadDynamicSectionFromMemory();
  bool ReadPadSegmentNote();
  bool ReserveAddressSpace(address_space_params *address_space);
  [[nodiscard]] bool MapSegment(size_t seg_idx, size_t len);
  [[nodiscard]] bool CompatMapSegment(size_t seg_idx, size_t len);
  void ZeroFillSegment(const ElfW(Phdr) * phdr);
  void DropPaddingPages(const ElfW(Phdr) * phdr, uint64_t seg_file_end);
  [[nodiscard]] bool MapBssSection(const ElfW(Phdr) * phdr, ElfW(Addr) seg_page_end, ElfW(Addr) seg_file_end);
  [[nodiscard]] bool IsEligibleFor16KiBAppCompat(ElfW(Addr) * vaddr);
  [[nodiscard]] bool HasAtMostOneRelroSegment(const ElfW(Phdr) * *relro_phdr);
  [[nodiscard]] bool Setup16KiBAppCompat();
  bool LoadSegments();
  bool FindPhdr();
  bool FindGnuPropertySection();
  bool CheckPhdr(ElfW(Addr));
  bool CheckFileRange(ElfW(Addr) offset, size_t size, size_t alignment);

  off64_t FileOffsetToVirtualOffset(off64_t offset);

  bool did_read_ = false;
  bool did_load_ = false;
  bool did_disk_load_ = false;
  bool did_disk_read_ = false;
  std::string name_;
  int fd_;
  off64_t file_offset_;
  off64_t file_size_;

public:
  ElfW(Ehdr) header_;
  size_t phdr_num_;

  MappedFileFragment phdr_fragment_;
  const ElfW(Phdr) * phdr_table_;

  MappedFileFragment shdr_fragment_;
  const ElfW(Shdr) * shdr_table_;
  size_t shdr_num_;

  MappedFileFragment dynamic_fragment_;
  const ElfW(Dyn) * dynamic_;

  MappedFileFragment strtab_fragment_;
  const char *strtab_;
  size_t strtab_size_;

  // First page of reserved address space.
  void *load_start_;
  // Size in bytes of reserved address space.
  size_t load_size_;
  // First page of inaccessible gap mapping reserved for this DSO.
  void *gap_start_;
  // Size in bytes of the gap mapping.
  size_t gap_size_;
  // Load bias.
  // Library load base address, usually same as load_start_ because first page typically has virtual address 0
  ElfW(Addr) load_bias_;

  // Loaded phdr.
  const ElfW(Phdr) * loaded_phdr_;

  // Is map owned by the caller
  bool mapped_by_caller_;

  // Pad gaps between segments when memory mapping?
  bool should_pad_segments_ = false;

  // Use app compat mode when loading 4KiB max-page-size ELFs on 16KiB page-size devices?
  bool should_use_16kib_app_compat_ = false;

  // RELRO region for 16KiB compat loading
  ElfW(Addr) compat_relro_start_ = 0;
  ElfW(Addr) compat_relro_size_ = 0;

  // Only used by AArch64 at the moment.
  GnuPropertySection note_gnu_property_ __unused;

  ElfW(Sym) * symtab_;
  size_t symtab_size_;
  int pltrel_type_;

  ELF_REL *plt_rel_;
  size_t plt_rel_count_;

  // ELF HASH
  size_t nbucket_ = 0;
  size_t nchain_;
  uint32_t *bucket_;
  uint32_t *chain_;

  // GNU HASH
  size_t gnu_nbucket_ = 0;
  uint32_t gnu_symbias_;
  uint32_t *gnu_bucket_;
  uint32_t *gnu_chain_;
  uint32_t gnu_maskwords_;
  uint32_t gnu_shift2_;
  ElfW(Addr) * gnu_bloom_filter_;

  std::string real_path_;
  std::unique_ptr<ElfDiskInfo> disk_info_;
};

size_t phdr_table_get_load_size(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) *min_vaddr = nullptr,
                                ElfW(Addr) *max_vaddr = nullptr);

size_t phdr_table_get_maximum_alignment(const ElfW(Phdr) * phdr_table, size_t phdr_count);

int phdr_table_protect_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                bool should_pad_segments, bool should_use_16kib_app_compat,
                                const GnuPropertySection *prop __unused = nullptr);

int phdr_table_unprotect_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                  bool should_pad_segments, bool should_use_16kib_app_compat);

int phdr_table_protect_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                 bool should_pad_segments, bool should_use_16kib_app_compat);

int phdr_table_unprotect_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                   bool should_pad_segments, bool should_use_16kib_app_compat);

int phdr_table_serialize_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias, int fd,
                                   size_t *file_offset);

int phdr_table_map_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias, int fd,
                             size_t *file_offset);

#if defined(__arm__)
int phdr_table_get_arm_exidx(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                             ElfW(Addr) * *arm_exidx, size_t *arm_exidix_count);
#endif

void phdr_table_get_dynamic_section(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                    ElfW(Dyn) * *dynamic, ElfW(Word) * dynamic_flags);

const char *phdr_table_get_interpreter_name(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

bool page_size_migration_supported();

int remap_memtag_globals_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

void protect_memtag_globals_ro_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

void name_memtag_globals_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                  const char *soname, std::list<std::string> *vma_names);

bool get_16kb_appcompat_mode();
} // namespace fakelinker
