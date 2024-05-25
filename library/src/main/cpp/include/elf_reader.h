#pragma once

#include <string>
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
  Address section_strtab_addr = 0; // 实际内存中的地址
  uintptr_t section_strtab_size;   // 节区大小
  uintptr_t str_addralign;

  Address section_symtab_offset;
  Address section_symtab_addr = 0;
  uintptr_t section_symtab_size;
  uintptr_t sym_entsize;
  uintptr_t sym_addralign;
  uintptr_t sym_num;

  unique_memory mmap_memory;
  unique_fd library_fd;
  Address base;
};


class ElfReader {
public:
  ElfReader();

  bool Read(const char *name, int fd, off64_t file_offset, off64_t file_size);
  bool ReadFormDisk(int fd, off64_t file_size);
  bool Load(address_space_params *address_space);
  bool LoadFromMemory(const char *name);
  bool LoadFromDisk(const char *library_name);

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

  uint64_t FindInternalSymbol(const char *name, bool useRegex = false);

  /**
   * 查找内部符号地址,可支持正则表达式,正则表达式使用默认 std::regex 因此需要调用者保证正则表达式合法,
   * 注意: 虽然支持正则表达式但也优先匹配符号名称,这是方便完全符号匹配与正则匹配一起查找,避免多次遍历符号表
   *
   * @param symbols  查找的内部符号名称或正则表达式集合,保证非空避免查找整个符号表
   * @param useRegex 开启正则匹配支持
   * @return         返回地址集合,找到则对应地址非0
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
  bool ReserveAddressSpace(address_space_params *address_space);
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
  // 库加载基址,通常就是 load_start_ 因为第一个页通常虚拟地址为0
  ElfW(Addr) load_bias_;

  // Loaded phdr.
  const ElfW(Phdr) * loaded_phdr_;

  // Is map owned by the caller
  bool mapped_by_caller_;

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
                                const GnuPropertySection *prop = nullptr);

int phdr_table_unprotect_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

int phdr_table_protect_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias);

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

} // namespace fakelinker
