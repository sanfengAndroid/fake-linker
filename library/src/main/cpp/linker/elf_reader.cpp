// https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linker_phdr.cpp;bpv=0;bpt=1

#include "fakelinker/elf_reader.h"

#include <android/api-level.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <unistd.h>

#include <cinttypes>
#include <regex>

#include <fakelinker/linker.h>
#include <fakelinker/maps_util.h>

#include "linker_util.h"
#include "xz/xz.h"

#define DL_ERR(...)                 LOGE(__VA_ARGS__)
#define DL_WARN(...)                LOGW(__VA_ARGS__)
#define DL_ERR_AND_LOG(...)         LOGE(__VA_ARGS__)

#define NT_ANDROID_TYPE_IDENT       1
#define NT_ANDROID_TYPE_KUSER       3
#define NT_ANDROID_TYPE_MEMTAG      4
#define NT_ANDROID_TYPE_PAD_SEGMENT 5

constexpr unsigned kLibraryAlignmentBits = 18;
constexpr size_t kLibraryAlignment = 1UL << kLibraryAlignmentBits;
static constexpr size_t kCompatPageSize = 0x1000;

namespace fakelinker {


static bool __get_elf_note(unsigned note_type, const char *note_name, const ElfW(Addr) note_addr,
                           const ElfW(Phdr) * phdr_note, const ElfW(Nhdr) * *note_hdr, const char **note_desc) {
  if (phdr_note->p_type != PT_NOTE || !note_name || !note_addr) {
    return false;
  }

  ElfW(Addr) p = note_addr;
  ElfW(Addr) note_end = p + phdr_note->p_memsz;

  while (p + sizeof(ElfW(Nhdr)) <= note_end) {
    const ElfW(Nhdr) *note = reinterpret_cast<const ElfW(Nhdr) *>(p);
    p += sizeof(ElfW(Nhdr));
    const char *name = reinterpret_cast<const char *>(p);
    p += align_up(note->n_namesz, 4);
    const char *desc = reinterpret_cast<const char *>(p);
    p += align_up(note->n_descsz, 4);
    if (p > note_end) {
      break;
    }
    if (note->n_type != note_type) {
      continue;
    }
    size_t note_name_len = strlen(note_name) + 1;
    if (note->n_namesz != note_name_len || strncmp(note_name, name, note_name_len) != 0) {
      break;
    }

    *note_hdr = note;
    *note_desc = desc;

    return true;
  }
  return false;
}

static bool __find_elf_note(unsigned int note_type, const char *note_name, const ElfW(Phdr) * phdr_start,
                            size_t phdr_ct, const ElfW(Nhdr) * *note_hdr, const char **note_desc,
                            const ElfW(Addr) load_bias) {
  for (size_t i = 0; i < phdr_ct; ++i) {
    const ElfW(Phdr) *phdr = &phdr_start[i];

    ElfW(Addr) note_addr = load_bias + phdr->p_vaddr;
    if (__get_elf_note(note_type, note_name, note_addr, phdr, note_hdr, note_desc)) {
      return true;
    }
  }

  return false;
}

static std::string GetProperty(const std::string &key, const std::string &default_value) {
#if __ANDROID_API__ >= __ANDROID_API_O__
  std::string property_value;
  const prop_info *pi = __system_property_find(key.c_str());
  if (pi == nullptr)
    return default_value;

  __system_property_read_callback(
    pi,
    [](void *cookie, const char *, const char *value, unsigned) {
      auto property_value = reinterpret_cast<std::string *>(cookie);
      *property_value = value;
    },
    &property_value);
  // If the property exists but is empty, also return the default value.
  // Since we can't remove system properties, "empty" is traditionally
  // the same as "missing" (this was true for cutils' property_get).
  return property_value.empty() ? default_value : property_value;
#else
  char value[92] = {0};
  if (__system_property_get(key.c_str(), value) < 1) {
    return default_value;
  }
  return value;
#endif
}

static bool GetBoolProperty(const std::string &key, bool default_value) {
  std::string s = GetProperty(key, "");
  if (s == "1" || s == "y" || s == "yes" || s == "on" || s == "true") {
    return true;
  }
  if (s == "0" || s == "n" || s == "no" || s == "off" || s == "false") {
    return false;
  }
  return default_value;
}

static int GetTargetElfMachine() {
#if defined(__arm__)
  return EM_ARM;
#elif defined(__aarch64__)
  return EM_AARCH64;
#elif defined(__i386__)
  return EM_386;
#elif defined(__x86_64__)
  return EM_X86_64;
#endif
}

static constexpr bool isUseRela() {
#ifdef USE_RELA
  return true;
#else
  return false;
#endif
}

/**
  TECHNICAL NOTE ON ELF LOADING.

  An ELF file's program header table contains one or more PT_LOAD
  segments, which corresponds to portions of the file that need to
  be mapped into the process' address space.

  Each loadable segment has the following important properties:

    p_offset  -> segment file offset
    p_filesz  -> segment file size
    p_memsz   -> segment memory size (always >= p_filesz)
    p_vaddr   -> segment's virtual address
    p_flags   -> segment flags (e.g. readable, writable, executable)
    p_align   -> segment's in-memory and in-file alignment

  We will ignore the p_paddr field of ElfW(Phdr) for now.

  The loadable segments can be seen as a list of [p_vaddr ... p_vaddr+p_memsz)
  ranges of virtual addresses. A few rules apply:

  - the virtual address ranges should not overlap.

  - if a segment's p_filesz is smaller than its p_memsz, the extra bytes
    between them should always be initialized to 0.

  - ranges do not necessarily start or end at page boundaries. Two distinct
    segments can have their start and end on the same page. In this case, the
    page inherits the mapping flags of the latter segment.

  Finally, the real load addrs of each segment is not p_vaddr. Instead the
  loader decides where to load the first segment, then will load all others
  relative to the first one to respect the initial range layout.

  For example, consider the following list:

    [ offset:0,      filesz:0x4000, memsz:0x4000, vaddr:0x30000 ],
    [ offset:0x4000, filesz:0x2000, memsz:0x8000, vaddr:0x40000 ],

  This corresponds to two segments that cover these virtual address ranges:

       0x30000...0x34000
       0x40000...0x48000

  If the loader decides to load the first segment at address 0xa0000000
  then the segments' load address ranges will be:

       0xa0030000...0xa0034000
       0xa0040000...0xa0048000

  In other words, all segments must be loaded at an address that has the same
  constant offset from their p_vaddr value. This offset is computed as the
  difference between the first segment's load address, and its p_vaddr value.

  However, in practice, segments do _not_ start at page boundaries. Since we
  can only memory-map at page boundaries, this means that the bias is
  computed as:

       load_bias = phdr0_load_address - PAGE_START(phdr0->p_vaddr)

  (NOTE: The value must be used as a 32-bit unsigned integer, to deal with
          possible wrap around UINT32_MAX for possible large p_vaddr values).

  And that the phdr0_load_address must start at a page boundary, with
  the segment's real content starting at:

       phdr0_load_address + PAGE_OFFSET(phdr0->p_vaddr)

  Note that ELF requires the following condition to make the mmap()-ing work:

      PAGE_OFFSET(phdr0->p_vaddr) == PAGE_OFFSET(phdr0->p_offset)

  The load_bias must be added to any p_vaddr value read from the ELF file to
  determine the corresponding memory address.

 **/


// Default PMD size for x86_64 and aarch64 (2MB).
static constexpr size_t kPmdSize = (1UL << 21);

ElfReader::ElfReader() :
    did_read_(false), did_load_(false), fd_(-1), file_offset_(0), file_size_(0), phdr_num_(0), phdr_table_(nullptr),
    shdr_table_(nullptr), shdr_num_(0), dynamic_(nullptr), strtab_(nullptr), strtab_size_(0), load_start_(nullptr),
    load_size_(0), load_bias_(0), loaded_phdr_(nullptr), mapped_by_caller_(false) {}

bool ElfReader::Read(const char *name, int fd, off64_t file_offset, off64_t file_size) {
  if (did_read_) {
    return true;
  }
  name_ = name;
  fd_ = fd;
  file_offset_ = file_offset;
  file_size_ = file_size;

  if (ReadElfHeader() && VerifyElfHeader() && ReadProgramHeaders() && ReadSectionHeaders() && ReadDynamicSection() &&
      ReadPadSegmentNote()) {
    did_read_ = true;
  }
  if (page_size() == 0x4000 && phdr_table_get_maximum_alignment(phdr_table_, phdr_num_) == 0x1000) {
    // This prop needs to be read on 16KiB devices for each ELF where min_palign is 4KiB.
    // It cannot be cached since the developer may toggle app compat on/off.
    // This check will be removed once app compat is made the default on 16KiB devices.
    should_use_16kib_app_compat_ =
      GetBoolProperty("bionic.linker.16kb.app_compat.enabled", false) || get_16kb_appcompat_mode();
  }

  return did_read_;
}

bool ElfReader::ReadFormDisk(int fd, off64_t file_size) {
  if (did_disk_read_) {
    return true;
  }
  fd_ = fd;
  file_offset_ = 0;
  file_size_ = file_size;
  if (ReadElfHeader() && VerifyElfHeader() && ReadProgramHeaders(false) && ReadSectionHeaders(false)) {
    did_disk_read_ = true;
  }
  return did_disk_read_;
}

bool ElfReader::Load(address_space_params *address_space) {
  if (!did_read_) {
    return false;
  }
  if (did_load_) {
    return true;
  }
  bool reserveSuccess = ReserveAddressSpace(address_space);
  if (reserveSuccess && LoadSegments() && FindPhdr() && FindGnuPropertySection()) {
    did_load_ = true;
#if defined(__aarch64__)
    // For Armv8.5-A loaded executable segments may require PROT_BTI.
    if (note_gnu_property_.IsBTICompatible()) {
      did_load_ = (phdr_table_protect_segments(phdr_table_, phdr_num_, load_bias_, should_pad_segments_,
                                               should_use_16kib_app_compat_, &note_gnu_property_) == 0);
    }
#endif
  }

  if (reserveSuccess && !did_load_) {
    if (load_start_ != nullptr && load_size_ != 0) {
      if (!mapped_by_caller_) {
        munmap(load_start_, load_size_);
      }
    }
  }

  return did_load_;
}

bool ElfReader::LoadFromMemory(const char *name) {
  if (did_load_) {
    return true;
  }
  MapsHelper maps;
  if (!maps.GetLibraryProtect(name)) {
    DL_ERR("find library failed: %s", name);
    return false;
  }
  Address base = maps.GetLibraryBaseAddress();
  if (base == 0) {
    DL_ERR("get library base address failed: %s", name);
    return false;
  }
  real_path_ = maps.GetCurrentRealPath();
  load_start_ = reinterpret_cast<void *>(base);
  const ElfW(Ehdr) *ehdr = reinterpret_cast<const ElfW(Ehdr) *>(base);
  header_ = *ehdr;
  // Calculate load_bias_, when loading from memory, only read dynamic sections
  if (ReadProgramHeadersFromMemory(reinterpret_cast<const char *>(ehdr), maps) && ReadDynamicSectionFromMemory()) {
    did_load_ = true;
  }
  return did_load_;
}

bool ElfReader::LoadFromDisk(const char *library_name) {
  if (did_disk_load_) {
    return true;
  }
  if (disk_info_.get() != nullptr) {
    return false;
  }
  MapsHelper maps(library_name);
  Address base = maps.GetLibraryBaseAddress();
  if (base == 0) {
    DL_ERR("read library base address failed: %s", library_name);
    return false;
  }
  load_bias_ = static_cast<ElfW(Addr)>(base);
  std::string real_path = maps.GetCurrentRealPath();
  name_ = library_name;
  disk_info_ = std::make_unique<ElfDiskInfo>();
  disk_info_->library_fd.reset(open64(real_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!disk_info_->library_fd.ok()) {
    DL_ERR("Failed to open file from disk: %s", real_path.c_str());
    return false;
  }
  size_t file_size = lseek64(disk_info_->library_fd.get(), 0, SEEK_END);
  if (!ReadFormDisk(disk_info_->library_fd.get(), file_size)) {
    return false;
  }

  void *addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, disk_info_->library_fd.get(), 0);
  if (addr == MAP_FAILED) {
    DL_ERR("mmap file %s failed", real_path.c_str());
    return false;
  }

  disk_info_->mmap_memory.reset(addr, file_size, true);

  // Read file section data
  ElfW(Ehdr) *head = disk_info_->mmap_memory.get<ElfW(Ehdr)>();
  if (head == nullptr) {
    DL_ERR("read elf header failed: %s", library_name);
    return false;
  }

  auto ParseSection = [this](ElfW(Ehdr) * head, size_t file_size) {
    char *section_header = reinterpret_cast<char *>(head) + head->e_shoff;
    auto shstr_section = reinterpret_cast<ElfW(Shdr) *>(section_header + sizeof(ElfW(Shdr)) * head->e_shstrndx);
    char *shstr_table = reinterpret_cast<char *>(head) + shstr_section->sh_offset;
    auto section = reinterpret_cast<ElfW(Shdr) *>(section_header);

    for (int i = 0; i < head->e_shnum; ++i, ++section) {
      switch (section->sh_type) {
      case SHT_STRTAB:
        if (strcmp(".strtab", shstr_table + section->sh_name) == 0) {
          disk_info_->section_strtab_addr =
            reinterpret_cast<Address>(reinterpret_cast<char *>(head) + section->sh_offset);
          disk_info_->section_strtab_offset = section->sh_offset;
          disk_info_->section_strtab_size = section->sh_size;
          if (section->sh_offset + section->sh_size > file_size) {
            DL_ERR("%s strtab section 0x%" PRIx64 " ~ 0x%" PRIx64 " out of file range 0x%zx", name(),
                   disk_info_->section_strtab_offset,
                   disk_info_->section_strtab_offset + disk_info_->section_strtab_size, file_size);
            return false;
          }
        }
        break;
      case SHT_SYMTAB:
        disk_info_->section_symtab_addr =
          reinterpret_cast<Address>(reinterpret_cast<char *>(head) + section->sh_offset);
        disk_info_->sym_num = section->sh_size / sizeof(ElfW(Sym));
        if (section->sh_entsize != sizeof(ElfW(Sym))) {
          LOGW("%s elf symtab section e_shentsize error, file set: 0x%" PRIx32 ", expect size: 0x%" PRIx32, name(),
               static_cast<uint32_t>(section->sh_entsize), static_cast<uint32_t>(sizeof(ElfW(Sym))));
        }
        disk_info_->sym_entsize = sizeof(ElfW(Sym));
        disk_info_->section_symtab_offset = section->sh_offset;

        if (section->sh_offset + section->sh_size > file_size) {
          DL_ERR("%s symtab section 0x%" PRIx64 " ~ 0x%" PRIx64 " out of file range 0x%zx", name(),
                 disk_info_->section_symtab_offset, disk_info_->section_symtab_offset + disk_info_->section_symtab_size,
                 file_size);
          return false;
        }
        break;
      case SHT_PROGBITS: {
        if (strcmp(".gnu_debugdata", shstr_table + section->sh_name) == 0) {
          disk_info_->section_debugdata_addr =
            reinterpret_cast<Address>(reinterpret_cast<char *>(head) + section->sh_offset);
          disk_info_->section_debugdata_size = section->sh_size;
        }
      }
      default:
        break;
      }
    }
    return true;
  };

  if (!ParseSection(head, file_size)) {
    return false;
  }
  if ((disk_info_->section_strtab_addr == 0 || disk_info_->section_symtab_addr == 0) &&
      disk_info_->section_debugdata_addr != 0) {
    LOGD("parse debugdata section %s", name());
    if (DecompressDebugData() &&
        ParseSection(reinterpret_cast<ElfW(Ehdr) *>(disk_info_->debugdata.data()), disk_info_->debugdata.size())) {
      disk_info_->mmap_memory.reset();
    }
  }

  if (disk_info_->section_strtab_addr == 0 || disk_info_->section_symtab_addr == 0) {
    DL_ERR(
      "%s elf not found strtab section: 0x%" PRIx64 " or symtab section: 0x%" PRIx64 ", debugdata section: 0x%" PRIx64,
      name(), disk_info_->section_strtab_addr, disk_info_->section_symtab_addr, disk_info_->section_debugdata_addr);
    return false;
  }
  LOGD("parse elf %s strtab section:0x%" PRIx64 ", symtab section: 0x%" PRIx64, name(), disk_info_->section_strtab_addr,
       disk_info_->section_symtab_addr);
  did_disk_load_ = true;
  return true;
}

bool ElfReader::CacheInternalSymbols() {
  if (!did_disk_load_) {
    return false;
  }
  if (!disk_info_->internal_symbols.empty()) {
    return true;
  }
  IterateInternalSymbols([&](std::string_view symbol_name, const ElfW(Sym) * sym) {
    auto st_type = ELF_ST_TYPE(sym->st_info);
    if ((st_type == STT_FUNC || st_type == STT_OBJECT) && sym->st_size) {
      disk_info_->internal_symbols.emplace(symbol_name, sym);
    }
    return false;
  });
  return true;
}

bool ElfReader::DecompressDebugData() {
  if (disk_info_->section_debugdata_addr == 0) {
    return false;
  }
  xz_crc32_init();
#ifdef XZ_USE_CRC64
  xz_crc64_init();
#endif
  xz_dec *dec = xz_dec_init(XZ_DYNALLOC, 1 << 26);
  if (!dec) {
    LOGE("xz_dec_init failed");
    return false;
  }
  xz_buf stream;
  stream.in = reinterpret_cast<uint8_t *>(static_cast<uintptr_t>(disk_info_->section_debugdata_addr));
  stream.in_pos = 0;
  stream.in_size = disk_info_->section_debugdata_size;

  std::string buffer;
  const int chunk_size = 1024 * 1024;
  buffer.resize(chunk_size);
  stream.out = reinterpret_cast<uint8_t *>(buffer.data());
  stream.out_pos = 0;
  stream.out_size = 1024 * 1024;
  xz_ret ret = XZ_OK;
  bool success = true;
  do {
    ret = xz_dec_run(dec, &stream);
    if (ret == XZ_OK || ret == XZ_STREAM_END) {
      if (stream.out_pos == buffer.size()) {
        buffer.resize(buffer.size() + chunk_size);
        stream.out = reinterpret_cast<uint8_t *>(buffer.data());
        stream.out_size = buffer.size();
      }
    } else if (ret == XZ_DATA_ERROR) {
      LOGE("Data integrity error during decompression!");
      success = false;
      break;
    } else {
      LOGE("Decompression failed with error code: %d", ret);
      success = false;
      break;
    }
  } while (ret == XZ_OK);
  if (success && ret == XZ_STREAM_END) {
    buffer.resize(stream.out_pos);
    disk_info_->debugdata = std::move(buffer);
  } else {
    success = false;
  }
  xz_dec_end(dec);
  return success;
}

const ElfW(Sym) * ElfReader::GnuHashLookupSymbol(const char *name) {
  if (!name) {
    return nullptr;
  }
  std::string symbol_name = name;
  if (symbol_name.empty()) {
    return nullptr;
  }
  const uint32_t hash = calculate_gnu_hash(name);

  constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
  const uint32_t word_num = (hash / kBloomMaskBits) & gnu_maskwords_;
  const ElfW(Addr) bloom_word = gnu_bloom_filter_[word_num];
  const uint32_t h1 = hash % kBloomMaskBits;
  const uint32_t h2 = (hash >> gnu_shift2_) % kBloomMaskBits;

  if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) == 0) {
    return nullptr;
  }

  ElfW(Sym) * sym;
  uint32_t n = gnu_bucket_[hash % gnu_nbucket_];
  if (n == 0) {
    return nullptr;
  }
  do {
    ElfW(Sym) *s = symtab_ + n;
    if (((gnu_chain_[n] ^ hash) >> 1) == 0 && symbol_name == get_string(s->st_name)) {
      return s;
    }
  } while ((gnu_chain_[n++] & 1) == 0);
  return nullptr;
}

const ElfW(Sym) * ElfReader::ElfHashLookupSymbol(const char *name) {
  if (!name) {
    return nullptr;
  }
  std::string symbol_name = name;
  if (symbol_name.empty()) {
    return nullptr;
  }
  uint32_t hash = calculate_elf_hash(name);
  for (uint32_t n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
    ElfW(Sym) *s = symtab_ + n;
    if (symbol_name == get_string(s->st_name)) {
      return s;
    }
  }
  return nullptr;
}

uint64_t ElfReader::FindImportSymbol(const char *name) {
  if (!did_load_ || !name) {
    return 0;
  }
  std::string symbol_name = name;
  if (symbol_name.empty()) {
    return 0;
  }
  // Prefer using ELF hash to calculate symbol index
  const ElfW(Sym) *sym = nullptr;
  if (nbucket_ != 0) {
    sym = ElfHashLookupSymbol(name);
  } else if (gnu_symbias_ != 0) {
    for (int i = 0; i < gnu_symbias_; ++i) {
      ElfW(Sym) *s = symtab_ + i;
      if (symbol_name == get_string(s->st_name)) {
        sym = s;
        break;
      }
    }
  } else {
    DL_ERR("unreachable, no symbol hash table?");
    return 0;
  }
  if (sym == nullptr) {
    return 0;
  }
  size_t sym_index = (reinterpret_cast<uint64_t>(sym) - reinterpret_cast<uint64_t>(symtab_)) / sizeof(ElfW(Sym));
  ELF_REL *start = plt_rel_;
  ELF_REL *end = start + plt_rel_count_;
  for (ELF_REL *rel = start; rel != end; ++rel) {
    if (R_SYM(rel->r_info) == sym_index) {
      return load_bias_ + rel->r_offset;
    }
  }
  return 0;
}

std::vector<Address> ElfReader::FindImportSymbols(const std::vector<std::string> &symbols) {
  std::vector<Address> ret;
  size_t num = symbols.size();
  ret.resize(num, 0);
  if (symbols.empty() || !did_load_) {
    return ret;
  }

  std::vector<size_t> sym_indexs;
  size_t found = 0;
  if (nbucket_ != 0) {
    for (auto &symbol : symbols) {
      auto sym = ElfHashLookupSymbol(symbol.c_str());
      if (sym == nullptr) {
        sym_indexs.push_back(SIZE_MAX);
      } else {
        ++found;
        sym_indexs.push_back((reinterpret_cast<uint64_t>(sym) - reinterpret_cast<uint64_t>(symtab_)) /
                             sizeof(ElfW(Sym)));
      }
    }
  } else if (gnu_maskwords_ != 0) {
    sym_indexs.resize(num, SIZE_MAX);
    for (int i = 1; i < gnu_symbias_; ++i) {
      ElfW(Sym) *s = symtab_ + i;
      const char *name = get_string(s->st_name);
      for (size_t index = 0; index < num; ++index) {
        if (sym_indexs[index] == SIZE_MAX && symbols[index] == name) {
          sym_indexs[index] = i;
          ++found;
          break;
        }
      }
      if (found == num) {
        break;
      }
    }
  } else {
    DL_ERR("unreachable, no symbol hash table?");
    return ret;
  }

  ELF_REL *start = plt_rel_;
  ELF_REL *end = start + plt_rel_count_;
  size_t count = 0;
  for (ELF_REL *rel = start; rel != end; ++rel) {
    for (int i = 0; i < num; ++i) {
      if (ret[i] == 0 && sym_indexs[i] != SIZE_MAX && R_SYM(rel->r_info) == sym_indexs[i]) {
        ++count;
        ret[i] = load_bias_ + rel->r_offset;
        break;
      }
    }
    if (count == found) {
      break;
    }
  }
  return ret;
}

uint64_t ElfReader::FindExportSymbol(const char *name) {
  if (!did_load_ || !name) {
    return 0;
  }
  auto *sym = gnu_nbucket_ != 0 ? GnuHashLookupSymbol(name) : ElfHashLookupSymbol(name);
  if (sym == nullptr) {
    return 0;
  }
  return load_bias_ + sym->st_value;
}

std::vector<Address> ElfReader::FindExportSymbols(const std::vector<std::string> &symbols) {
  std::vector<Address> ret;
  if (symbols.empty() || !did_load_) {
    ret.resize(symbols.size(), 0);
    return ret;
  }
  for (auto &symbol : symbols) {
    ret.push_back(FindExportSymbol(symbol.c_str()));
  }
  return ret;
}

bool ElfReader::IterateInternalSymbols(const std::function<bool(std::string_view, const ElfW(Sym) *)> &callback) {
  if (!did_disk_load_) {
    return false;
  }
  auto sym_start = reinterpret_cast<ElfW(Sym) *>(disk_info_->section_symtab_addr);
  auto sym_end = reinterpret_cast<ElfW(Sym) *>(disk_info_->section_symtab_addr) + disk_info_->sym_num;
  for (ElfW(Sym) *sym = sym_start; sym != sym_end; ++sym) {
    auto sym_name = reinterpret_cast<const char *>(disk_info_->section_strtab_addr + sym->st_name);
    if (callback(sym_name, sym)) {
      return true;
    }
  }
  return true;
}

uint64_t ElfReader::FindInternalSymbol(std::string_view name, bool useRegex) {
  if (!did_disk_load_) {
    return 0;
  }
  std::regex reg(useRegex ? name.data() : "");
  uint64_t result = 0;
  if (disk_info_->internal_symbols.empty()) {
    IterateInternalSymbols([&](std::string_view symbol_name, const ElfW(Sym) * sym) -> bool {
      if (name == symbol_name || (useRegex && std::regex_search(symbol_name.data(), reg))) {
        result = load_bias_ + sym->st_value;
        return true;
      }
      return false;
    });
  } else if (auto it = disk_info_->internal_symbols.find(name); it != disk_info_->internal_symbols.end()) {
    return load_bias_ + it->second->st_value;
  }
  return result;
}

uint64_t ElfReader::FindInternalSymbolByPrefix(std::string_view prefix) {
  if (!did_disk_load_ || prefix.empty()) {
    return 0;
  }
  uint64_t result = 0;
  if (disk_info_->internal_symbols.empty()) {
    IterateInternalSymbols([&](std::string_view symbol_name, const ElfW(Sym) * sym) -> bool {
      if (strstr(symbol_name.data(), prefix.data()) == symbol_name.data()) {
        result = load_bias_ + sym->st_value;
        return true;
      }
      return false;
    });
  } else {
    if (auto it = disk_info_->internal_symbols.lower_bound(prefix);
        it != disk_info_->internal_symbols.end() && strstr(it->first.data(), prefix.data()) == it->first.data()) {
      return load_bias_ + it->second->st_value;
    }
  }
  return result;
}

std::vector<Address> ElfReader::FindInternalSymbols(const std::vector<std::string> &symbols, bool useRegex) {
  std::vector<Address> ret;
  ret.resize(symbols.size(), 0);
  if (symbols.empty() || !did_disk_load_) {
    return ret;
  }
  auto sym_start = reinterpret_cast<ElfW(Sym) *>(disk_info_->section_symtab_addr);
  auto sym_end = reinterpret_cast<ElfW(Sym) *>(disk_info_->section_symtab_addr) + disk_info_->sym_num;

  size_t count = 0;
  size_t num = symbols.size();
  // No longer checking for empty strings, caller guarantees validity
  std::vector<std::regex> regs;
  if (useRegex) {
    for (auto &name : symbols) {
      // Regular expressions also need to be guaranteed valid by the caller
      regs.emplace_back(name);
    }
  }

  for (ElfW(Sym) *sym = sym_start; sym != sym_end; ++sym) {
    auto name = reinterpret_cast<const char *>(disk_info_->section_strtab_addr + sym->st_name);
    for (size_t index = 0; index < num; ++index) {
      if (ret[index] == 0 && (symbols[index] == name || (useRegex && std::regex_search(name, regs[index])))) {
        ++count;
        ret[index] = load_bias_ + sym->st_value;
        break;
      }
    }
    if (count == num) {
      break;
    }
  }
  return ret;
}

const char *ElfReader::get_string(ElfW(Word) index) const {
  if (strtab_ == nullptr || index >= strtab_size_) {
    return "";
  }
  return strtab_ + index;
}

bool ElfReader::ReadElfHeader() {
  ssize_t rc = TEMP_FAILURE_RETRY(pread64(fd_, &header_, sizeof(header_), file_offset_));
  if (rc < 0) {
    DL_ERR("can't read file \"%s\": %s", name_.c_str(), strerror(errno));
    return false;
  }

  if (rc != sizeof(header_)) {
    DL_ERR("\"%s\" is too small to be an ELF executable: only found %zd bytes", name_.c_str(), static_cast<size_t>(rc));
    return false;
  }
  return true;
}

static const char *EM_to_string(int em) {
  if (em == EM_386)
    return "EM_386";
  if (em == EM_AARCH64)
    return "EM_AARCH64";
  if (em == EM_ARM)
    return "EM_ARM";
  if (em == EM_X86_64)
    return "EM_X86_64";
  return "EM_???";
}

bool ElfReader::VerifyElfHeader() {
  if (memcmp(header_.e_ident, ELFMAG, SELFMAG) != 0) {
    DL_ERR("\"%s\" has bad ELF magic: %02x%02x%02x%02x", name_.c_str(), header_.e_ident[0], header_.e_ident[1],
           header_.e_ident[2], header_.e_ident[3]);
    return false;
  }

  // Try to give a clear diagnostic for ELF class mismatches, since they're
  // an easy mistake to make during the 32-bit/64-bit transition period.
  int elf_class = header_.e_ident[EI_CLASS];
#if defined(__LP64__)
  if (elf_class != ELFCLASS64) {
    if (elf_class == ELFCLASS32) {
      DL_ERR("\"%s\" is 32-bit instead of 64-bit", name_.c_str());
    } else {
      DL_ERR("\"%s\" has unknown ELF class: %d", name_.c_str(), elf_class);
    }
    return false;
  }
#else
  if (elf_class != ELFCLASS32) {
    if (elf_class == ELFCLASS64) {
      DL_ERR("\"%s\" is 64-bit instead of 32-bit", name_.c_str());
    } else {
      DL_ERR("\"%s\" has unknown ELF class: %d", name_.c_str(), elf_class);
    }
    return false;
  }
#endif

  if (header_.e_ident[EI_DATA] != ELFDATA2LSB) {
    DL_ERR("\"%s\" not little-endian: %d", name_.c_str(), header_.e_ident[EI_DATA]);
    return false;
  }

  if (header_.e_type != ET_DYN) {
    DL_ERR("\"%s\" has unexpected e_type: %d", name_.c_str(), header_.e_type);
    return false;
  }

  if (header_.e_version != EV_CURRENT) {
    DL_ERR("\"%s\" has unexpected e_version: %d", name_.c_str(), header_.e_version);
    return false;
  }

  if (header_.e_machine != GetTargetElfMachine()) {
    DL_ERR("\"%s\" is for %s (%d) instead of %s (%d)", name_.c_str(), EM_to_string(header_.e_machine),
           header_.e_machine, EM_to_string(GetTargetElfMachine()), GetTargetElfMachine());
    return false;
  }

  if (header_.e_shentsize != sizeof(ElfW(Shdr))) {
    // Fail if app is targeting Android O or above
    if (android_api >= 26) {
      DL_ERR_AND_LOG("\"%s\" has unsupported e_shentsize: 0x%x (expected 0x%zx)", name_.c_str(), header_.e_shentsize,
                     sizeof(ElfW(Shdr)));
      return false;
    }
    DL_WARN_documented_change(26, "invalid-elf-header_section-headers-enforced-for-api-level-26",
                              "\"%s\" has unsupported e_shentsize 0x%x (expected 0x%zx)", name_.c_str(),
                              header_.e_shentsize, sizeof(ElfW(Shdr)));
    add_dlwarning(name_.c_str(), "has invalid ELF header");
  }

  if (header_.e_shstrndx == 0) {
    // Fail if app is targeting Android O or above
    if (android_api >= 26) {
      DL_ERR_AND_LOG("\"%s\" has invalid e_shstrndx", name_.c_str());
      return false;
    }

    DL_WARN_documented_change(26, "invalid-elf-header_section-headers-enforced-for-api-level-26",
                              "\"%s\" has invalid e_shstrndx", name_.c_str());
    add_dlwarning(name_.c_str(), "has invalid ELF header");
  }

  return true;
}

bool ElfReader::CheckFileRange(ElfW(Addr) offset, size_t size, size_t alignment) {
  off64_t range_start;
  off64_t range_end;

  // Only header can be located at the 0 offset... This function called to
  // check DYNSYM and DYNAMIC sections and phdr/shdr - none of them can be
  // at offset 0.

  return offset > 0 && safe_add(&range_start, file_offset_, offset) && safe_add(&range_end, range_start, size) &&
    (range_start < file_size_) && (range_end <= file_size_) && ((offset % alignment) == 0);
}

off64_t ElfReader::FileOffsetToVirtualOffset(off64_t offset) {
  const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;
  for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_offset <= offset && offset - phdr->p_offset < phdr->p_filesz) {
      return phdr->p_vaddr + offset - phdr->p_offset;
    }
  }
  return -1;
}

// Loads the program header table from an ELF file into a read-only private
// anonymous mmap-ed block.
bool ElfReader::ReadProgramHeaders(bool map_memory) {
  phdr_num_ = header_.e_phnum;

  // Like the kernel, we only accept program header tables that
  // are smaller than 64KiB.
  if (phdr_num_ < 1 || phdr_num_ > 65536 / sizeof(ElfW(Phdr))) {
    DL_ERR("\"%s\" has invalid e_phnum: %zd", name_.c_str(), phdr_num_);
    return false;
  }

  // Boundary checks
  size_t size = phdr_num_ * sizeof(ElfW(Phdr));
  if (!CheckFileRange(header_.e_phoff, size, alignof(ElfW(Phdr)))) {
    DL_ERR_AND_LOG("\"%s\" has invalid phdr offset/size: %zu/%zu", name_.c_str(), static_cast<size_t>(header_.e_phoff),
                   size);
    return false;
  }
  if (!map_memory) {
    return true;
  }

  if (!phdr_fragment_.Map(fd_, file_offset_, header_.e_phoff, size)) {
    DL_ERR("\"%s\" phdr mmap failed: %s", name_.c_str(), strerror(errno));
    return false;
  }

  phdr_table_ = static_cast<ElfW(Phdr) *>(phdr_fragment_.data());
  return true;
}

bool ElfReader::ReadProgramHeadersFromMemory(const char *base, MapsHelper &maps) {
  phdr_num_ = header_.e_phnum;
  if (phdr_num_ < 1 || phdr_num_ > 65536 / sizeof(ElfW(Phdr))) {
    DL_ERR("\"%s\" has invalid e_phnum: %zd", name_.c_str(), phdr_num_);
    return false;
  }
  // Program header table is calculated based on the file header as the first load segment
  phdr_table_ = reinterpret_cast<const ElfW(Phdr) *>(base + header_.e_phoff);
  size_t size = phdr_num_ * sizeof(ElfW(Phdr));

  // 1. Verify memory accessibility
  if (!maps.CheckAddressPageProtect(reinterpret_cast<const Address>(phdr_table_), size, kMPRead)) {
    DL_ERR("The library's program header table is not accessible");
    return false;
  }
  // 2. Verify segment load offset
  const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;
  for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type == PT_PHDR) {
      load_bias_ = reinterpret_cast<ElfW(Addr)>(base);
      return phdr->p_vaddr == sizeof(ElfW(Ehdr));
    }
    if (phdr->p_type == PT_LOAD) {
      if (phdr->p_offset == 0) {
        load_bias_ = reinterpret_cast<ElfW(Addr)>(base - phdr->p_vaddr);
        return true;
      }
    }
  }
  return false;
}

bool ElfReader::ReadSectionHeaders(bool map_memory) {
  shdr_num_ = header_.e_shnum;

  if (shdr_num_ == 0) {
    DL_ERR_AND_LOG("\"%s\" has no section headers", name_.c_str());
    return false;
  }

  size_t size = shdr_num_ * sizeof(ElfW(Shdr));
  if (!CheckFileRange(header_.e_shoff, size, alignof(const ElfW(Shdr)))) {
    DL_ERR_AND_LOG("\"%s\" has invalid shdr offset/size: %zu/%zu", name_.c_str(), static_cast<size_t>(header_.e_shoff),
                   size);
    return false;
  }

  if (!map_memory) {
    return true;
  }

  if (!shdr_fragment_.Map(fd_, file_offset_, header_.e_shoff, size)) {
    DL_ERR("\"%s\" shdr mmap failed: %s", name_.c_str(), strerror(errno));
    return false;
  }

  shdr_table_ = static_cast<const ElfW(Shdr) *>(shdr_fragment_.data());
  return true;
}

/**
 * @brief Android requires reading sections and validation starting from 7.0
 *  SHT_DYNAMIC section must match PT_DYNAMIC segment, loading fails without dynamic section
 *
 * @return true
 * @return false
 */
bool ElfReader::ReadSectionHeadersFromMemory() {
  shdr_num_ = header_.e_shnum;
  if (shdr_num_ == 0) {
    DL_ERR_AND_LOG("\"%s\" has no section headers", name_.c_str());
    return false;
  }
  off64_t offset = FileOffsetToVirtualOffset(header_.e_shoff);
  if (offset == -1) {
    // Section was not loaded into memory
    return false;
  }
  shdr_table_ = reinterpret_cast<ElfW(Shdr) *>(load_bias_ + offset);
  // Section does not verify memory
  return true;
}

bool ElfReader::ReadDynamicSection() {
  // 1. Find .dynamic section (in section headers)
  const ElfW(Shdr) *dynamic_shdr = nullptr;
  for (size_t i = 0; i < shdr_num_; ++i) {
    if (shdr_table_[i].sh_type == SHT_DYNAMIC) {
      dynamic_shdr = &shdr_table_[i];
      break;
    }
  }

  if (dynamic_shdr == nullptr) {
    DL_ERR_AND_LOG("\"%s\" .dynamic section header was not found", name_.c_str());
    return false;
  }

  // Make sure dynamic_shdr offset and size matches PT_DYNAMIC phdr
  size_t pt_dynamic_offset = 0;
  size_t pt_dynamic_filesz = 0;
  for (size_t i = 0; i < phdr_num_; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table_[i];
    if (phdr->p_type == PT_DYNAMIC) {
      pt_dynamic_offset = phdr->p_offset;
      pt_dynamic_filesz = phdr->p_filesz;
    }
  }

  if (pt_dynamic_offset != dynamic_shdr->sh_offset) {
    if (android_api >= 26) {
      DL_ERR_AND_LOG("\"%s\" .dynamic section has invalid offset: 0x%zx, "
                     "expected to match PT_DYNAMIC offset: 0x%zx",
                     name_.c_str(), static_cast<size_t>(dynamic_shdr->sh_offset), pt_dynamic_offset);
      return false;
    }
    DL_WARN_documented_change(26, "invalid-elf-header_section-headers-enforced-for-api-level-26",
                              "\"%s\" .dynamic section has invalid offset: 0x%zx "
                              "(expected to match PT_DYNAMIC offset 0x%zx)",
                              name_.c_str(), static_cast<size_t>(dynamic_shdr->sh_offset), pt_dynamic_offset);
    add_dlwarning(name_.c_str(), "invalid .dynamic section");
  }

  if (pt_dynamic_filesz != dynamic_shdr->sh_size) {
    if (android_api >= 26) {
      DL_ERR_AND_LOG("\"%s\" .dynamic section has invalid size: 0x%zx, "
                     "expected to match PT_DYNAMIC filesz: 0x%zx",
                     name_.c_str(), static_cast<size_t>(dynamic_shdr->sh_size), pt_dynamic_filesz);
      return false;
    }
    DL_WARN_documented_change(26, "invalid-elf-header_section-headers-enforced-for-api-level-26",
                              "\"%s\" .dynamic section has invalid size: 0x%zx "
                              "(expected to match PT_DYNAMIC filesz 0x%zx)",
                              name_.c_str(), static_cast<size_t>(dynamic_shdr->sh_size), pt_dynamic_filesz);
    add_dlwarning(name_.c_str(), "invalid .dynamic section");
  }

  if (dynamic_shdr->sh_link >= shdr_num_) {
    DL_ERR_AND_LOG("\"%s\" .dynamic section has invalid sh_link: %d", name_.c_str(), dynamic_shdr->sh_link);
    return false;
  }

  const ElfW(Shdr) *strtab_shdr = &shdr_table_[dynamic_shdr->sh_link];

  if (strtab_shdr->sh_type != SHT_STRTAB) {
    DL_ERR_AND_LOG("\"%s\" .dynamic section has invalid link(%d) sh_type: %d "
                   "(expected SHT_STRTAB)",
                   name_.c_str(), dynamic_shdr->sh_link, strtab_shdr->sh_type);
    return false;
  }

  if (!CheckFileRange(dynamic_shdr->sh_offset, dynamic_shdr->sh_size, alignof(const ElfW(Dyn)))) {
    DL_ERR_AND_LOG("\"%s\" has invalid offset/size of .dynamic section", name_.c_str());
    return false;
  }

  if (!dynamic_fragment_.Map(fd_, file_offset_, dynamic_shdr->sh_offset, dynamic_shdr->sh_size)) {
    DL_ERR("\"%s\" dynamic section mmap failed: %s", name_.c_str(), strerror(errno));
    return false;
  }

  dynamic_ = static_cast<const ElfW(Dyn) *>(dynamic_fragment_.data());

  if (!CheckFileRange(strtab_shdr->sh_offset, strtab_shdr->sh_size, alignof(const char))) {
    DL_ERR_AND_LOG("\"%s\" has invalid offset/size of the .strtab section "
                   "linked from .dynamic section",
                   name_.c_str());
    return false;
  }

  if (!strtab_fragment_.Map(fd_, file_offset_, strtab_shdr->sh_offset, strtab_shdr->sh_size)) {
    DL_ERR("\"%s\" strtab section mmap failed: %s", name_.c_str(), strerror(errno));
    return false;
  }

  strtab_ = static_cast<const char *>(strtab_fragment_.data());
  strtab_size_ = strtab_fragment_.size();
  return true;
}

bool ElfReader::ReadDynamicSectionFromMemory() {
  LOGI("read dynamic segment form memory");
  const ElfW(Dyn) *dynamic = nullptr;
  for (size_t i = 0; i < phdr_num_; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table_[i];
    if (phdr->p_type == PT_DYNAMIC) {
      size_t pt_dynamic_offset = phdr->p_offset;
      size_t pt_dynamic_filesz = phdr->p_filesz;
      dynamic = reinterpret_cast<ElfW(Dyn) *>(load_bias_ + FileOffsetToVirtualOffset(phdr->p_offset));
    }
  }
  if (dynamic == nullptr) {
    DL_ERR_AND_LOG("\"%s\" .dynamic section header was not found", name_.c_str());
    return false;
  }

  ElfW(Rela) * rela;
  ElfW(Rel) * rel;
  uint8_t *android_relocs;
  size_t rela_count, rel_count, android_relocs_size;


  for (const ElfW(Dyn) *d = dynamic; d->d_tag != DT_NULL; ++d) {
    switch (d->d_tag) {
    case DT_HASH:
      nbucket_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[0];
      nchain_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[1];
      bucket_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr + 8);
      chain_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr + 8 + nbucket_ * 4);
      break;
    case DT_GNU_HASH:
      gnu_nbucket_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[0];
      gnu_symbias_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[1];
      gnu_maskwords_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[2];
      gnu_shift2_ = reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[3];
      gnu_bloom_filter_ = reinterpret_cast<ElfW(Addr) *>(load_bias_ + d->d_un.d_ptr + 16);
      gnu_bucket_ = reinterpret_cast<uint32_t *>(gnu_bloom_filter_ + gnu_maskwords_);
      gnu_chain_ = gnu_bucket_ + gnu_nbucket_ - reinterpret_cast<uint32_t *>(load_bias_ + d->d_un.d_ptr)[1];

      if (!powerof2(gnu_maskwords_)) {
        DL_ERR("invalid maskwords for gnu_hash = 0x%x, in \"%s\" expecting power to two", gnu_maskwords_, name());
        return false;
      }
      --gnu_maskwords_;
      break;
    case DT_STRTAB:
      strtab_ = reinterpret_cast<const char *>(load_bias_ + d->d_un.d_ptr);
      break;
    case DT_STRSZ:
      strtab_size_ = d->d_un.d_val;
      break;
    case DT_SYMTAB:
      symtab_ = reinterpret_cast<ElfW(Sym) *>(load_bias_ + d->d_un.d_ptr);
      break;
    case DT_SYMENT:
      if (d->d_un.d_val != sizeof(ElfW(Sym))) {
        DL_ERR("invalid DT_SYMENT: %zd in \"%s\"", static_cast<size_t>(d->d_un.d_val), name());
        return false;
      }
      break;
    case DT_PLTREL:
#ifdef USE_RELA
      if (d->d_un.d_val != DT_RELA) {
        DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_RELA", name());
        return false;
      }
#else
      if (d->d_un.d_val != DT_REL) {
        DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_REL", name());
        return false;
      }
#endif
      break;
    case DT_JMPREL:
      plt_rel_ = reinterpret_cast<ELF_REL *>(load_bias_ + d->d_un.d_ptr);
      break;
    case DT_PLTRELSZ:
      plt_rel_count_ = d->d_un.d_val / sizeof(ELF_REL);
      break;
    case DT_PLTGOT:
      break;
    case DT_DEBUG:
      break;
#ifdef USE_RELA
    case DT_RELA:
      rela = reinterpret_cast<ElfW(Rela) *>(load_bias_ + d->d_un.d_ptr);
      break;
    case DT_RELASZ:
      rela_count = d->d_un.d_val / sizeof(ElfW(Rela));
      break;
    case DT_ANDROID_RELA:
      android_relocs = reinterpret_cast<uint8_t *>(load_bias_ + d->d_un.d_ptr);
      break;
    case DT_ANDROID_RELASZ:
      android_relocs_size = d->d_un.d_val;
      break;
    case DT_ANDROID_REL:
      DL_ERR("unsupported DT_ANDROID_REL in \"%s\"", name());
      return false;
    case DT_ANDROID_RELSZ:
      DL_ERR("unsupported DT_ANDROID_RELSZ in \"%s\"", name());
      return false;
    case DT_RELAENT:
      if (d->d_un.d_val != sizeof(ElfW(Rela))) {
        DL_ERR("invalid DT_RELAENT: %zd", static_cast<size_t>(d->d_un.d_val));
        return false;
      }
      break;
    case DT_RELACOUNT:
      break;
    case DT_REL:
      DL_ERR("unsupported DT_REL in \"%s\"", name());
      return false;
    case DT_RELSZ:
      DL_ERR("unsupported DT_RELSZ in \"%s\"", name());
      return false;
#else
    case DT_REL:
      rel = reinterpret_cast<ElfW(Rel) *>(load_bias_ + d->d_un.d_ptr);
      break;

    case DT_RELSZ:
      rel_count = d->d_un.d_val / sizeof(ElfW(Rel));
      break;

    case DT_RELENT:
      if (d->d_un.d_val != sizeof(ElfW(Rel))) {
        DL_ERR("invalid DT_RELENT: %zd", static_cast<size_t>(d->d_un.d_val));
        return false;
      }
      break;

    case DT_ANDROID_REL:
      android_relocs = reinterpret_cast<uint8_t *>(load_bias_ + d->d_un.d_ptr);
      break;

    case DT_ANDROID_RELSZ:
      android_relocs_size = d->d_un.d_val;
      break;

    case DT_ANDROID_RELA:
      DL_ERR("unsupported DT_ANDROID_RELA in \"%s\"", name());
      return false;

    case DT_ANDROID_RELASZ:
      DL_ERR("unsupported DT_ANDROID_RELASZ in \"%s\"", name());
      return false;

    // "Indicates that all RELATIVE relocations have been concatenated together,
    // and specifies the RELATIVE relocation count."
    //
    // TODO: Spec also mentions that this can be used to optimize relocation process;
    // Not currently used by bionic linker - ignored.
    case DT_RELCOUNT:
      break;

    case DT_RELA:
      DL_ERR("unsupported DT_RELA in \"%s\"", name());
      return false;

    case DT_RELASZ:
      DL_ERR("unsupported DT_RELASZ in \"%s\"", name());
      return false;
#endif
    case DT_RELR:
    case DT_ANDROID_RELR:
      break;
    case DT_RELRSZ:
      break;
    case DT_ANDROID_RELRSZ:
      break;
    case DT_RELRENT:
      break;
    case DT_ANDROID_RELRENT:
      break;
    case DT_ANDROID_RELRCOUNT:
      break;
    case DT_INIT:
      break;
    case DT_FINI:
      break;
    case DT_INIT_ARRAY:
      break;
    case DT_INIT_ARRAYSZ:
      break;
    case DT_FINI_ARRAY:
      break;
    case DT_FINI_ARRAYSZ:
      break;
    case DT_PREINIT_ARRAY:
      break;
    case DT_PREINIT_ARRAYSZ:
      break;
    case DT_TEXTREL:
      break;
    case DT_SYMBOLIC:
      break;
    case DT_NEEDED:
      break;
    case DT_FLAGS:
      break;
    case DT_FLAGS_1:
      break;
    case DT_BIND_NOW:
      break;
    case DT_VERDEF:
      break;
    case DT_VERDEFNUM:
      break;
    case DT_VERNEED:
      break;
    case DT_VERNEEDNUM:
      break;
    case DT_RUNPATH:
      break;
    case DT_TLSDESC_GOT:
      break;
    case DT_TLSDESC_PLT:
      break;
    case DT_AARCH64_BTI_PLT:
      break;
    case DT_AARCH64_PAC_PLT:
      break;
    case DT_AARCH64_VARIANT_PCS:
      break;
    case DT_RPATH:
      break;
    // case DT_ENCODING:
    // break;
    case DT_LOOS:
      break;
    case DT_HIOS:
      break;
    case DT_LOPROC:
      break;
    case DT_HIPROC:
      break;
    default:
      break;
    }
  }
  if (nbucket_ == 0 && gnu_nbucket_ == 0) {
    LOGE("not found DT_HASH or DT_GNU_HASH, unable to find symbol, library: %s", name());
    return false;
  }
  if (symtab_ == nullptr) {
    LOGE("not found DT_SYMTAB, can't continue, library: %s", name());
    return false;
  }
  if (strtab_ == nullptr) {
    LOGE("not found DT_STRTAB, can't continue, library: %s", name());
    return false;
  }

  return true;
}

/* Returns the size of the extent of all the possibly non-contiguous
 * loadable segments in an ELF program header table. This corresponds
 * to the page-aligned size in bytes that needs to be reserved in the
 * process' address space. If there are no loadable segments, 0 is
 * returned.
 *
 * If out_min_vaddr or out_max_vaddr are not null, they will be
 * set to the minimum and maximum addresses of pages to be reserved,
 * or 0 if there is nothing to load.
 */
size_t phdr_table_get_load_size(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) * out_min_vaddr,
                                ElfW(Addr) * out_max_vaddr) {
  ElfW(Addr) min_vaddr = UINTPTR_MAX;
  ElfW(Addr) max_vaddr = 0;

  bool found_pt_load = false;
  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }
    found_pt_load = true;

    if (phdr->p_vaddr < min_vaddr) {
      min_vaddr = phdr->p_vaddr;
    }

    if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
      max_vaddr = phdr->p_vaddr + phdr->p_memsz;
    }
  }
  if (!found_pt_load) {
    min_vaddr = 0;
  }

  min_vaddr = PAGE_START(min_vaddr);
  max_vaddr = PAGE_END(max_vaddr);

  if (out_min_vaddr != nullptr) {
    *out_min_vaddr = min_vaddr;
  }
  if (out_max_vaddr != nullptr) {
    *out_max_vaddr = max_vaddr;
  }
  return max_vaddr - min_vaddr;
}

// Returns the maximum p_align associated with a loadable segment in the ELF
// program header table. Used to determine whether the file should be loaded at
// a specific virtual address alignment for use with huge pages.
size_t phdr_table_get_maximum_alignment(const ElfW(Phdr) * phdr_table, size_t phdr_count) {
  size_t maximum_alignment = page_size();

  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table[i];

    // p_align must be 0, 1, or a positive, integral power of two.
    if (phdr->p_type != PT_LOAD || ((phdr->p_align & (phdr->p_align - 1)) != 0)) {
      continue;
    }

    if (phdr->p_align > maximum_alignment) {
      maximum_alignment = phdr->p_align;
    }
  }

#if defined(__LP64__)
  return maximum_alignment;
#else
  return page_size();
#endif
}

// Returns the minimum p_align associated with a loadable segment in the ELF
// program header table. Used to determine if the program alignment is compatible
// with the page size of this system.
size_t phdr_table_get_minimum_alignment(const ElfW(Phdr) * phdr_table, size_t phdr_count) {
  size_t minimum_alignment = page_size();

  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table[i];

    // p_align must be 0, 1, or a positive, integral power of two.
    if (phdr->p_type != PT_LOAD || ((phdr->p_align & (phdr->p_align - 1)) != 0)) {
      continue;
    }

    if (phdr->p_align <= 1) {
      continue;
    }

    minimum_alignment = std::min(minimum_alignment, static_cast<size_t>(phdr->p_align));
  }

  return minimum_alignment;
}

// Reserve a virtual address range such that if it's limits were extended to the
// next 2**align boundary, it would not overlap with any existing mappings.
static void *ReserveWithAlignmentPadding(size_t size, size_t mapping_align, size_t start_align, void **out_gap_start,
                                         size_t *out_gap_size) {
  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
  // Reserve enough space to properly align the library's start address.
  mapping_align = std::max(mapping_align, start_align);
  if (mapping_align == page_size()) {
    void *mmap_ptr = mmap(nullptr, size, PROT_NONE, mmap_flags, -1, 0);
    if (mmap_ptr == MAP_FAILED) {
      return nullptr;
    }
    return mmap_ptr;
  }

  // Minimum alignment of shared library gap. For efficiency, this should match
  // the second level page size of the platform.
#if defined(__LP64__)
  constexpr size_t kGapAlignment = 1ul << 21; // 2MB
#else
  constexpr size_t kGapAlignment = 0;
#endif
  // Maximum gap size, in the units of kGapAlignment.
  constexpr size_t kMaxGapUnits = 32;
  // Allocate enough space so that the end of the desired region aligned up is
  // still inside the mapping.
  size_t mmap_size = align_up(size, mapping_align) + mapping_align - page_size();
  uint8_t *mmap_ptr = reinterpret_cast<uint8_t *>(mmap(nullptr, mmap_size, PROT_NONE, mmap_flags, -1, 0));
  if (mmap_ptr == MAP_FAILED) {
    return nullptr;
  }
  size_t gap_size = 0;
  size_t first_byte = reinterpret_cast<size_t>(align_up(mmap_ptr, mapping_align));
  size_t last_byte = reinterpret_cast<size_t>(align_down(mmap_ptr + mmap_size, mapping_align) - 1);
  if (kGapAlignment && first_byte / kGapAlignment != last_byte / kGapAlignment) {
    // This library crosses a 2MB boundary and will fragment a new huge page.
    // Lets take advantage of that and insert a random number of inaccessible
    // huge pages before that to improve address randomization and make it
    // harder to locate this library code by probing.
    munmap(mmap_ptr, mmap_size);
    mapping_align = std::max(mapping_align, kGapAlignment);
    gap_size = kGapAlignment * (is_first_stage_init() ? 1 : arc4random_uniform(kMaxGapUnits - 1) + 1);
    mmap_size = align_up(size + gap_size, mapping_align) + mapping_align - page_size();
    mmap_ptr = reinterpret_cast<uint8_t *>(mmap(nullptr, mmap_size, PROT_NONE, mmap_flags, -1, 0));
    if (mmap_ptr == MAP_FAILED) {
      return nullptr;
    }
  }

  uint8_t *gap_end, *gap_start;
  if (gap_size) {
    gap_end = align_down(mmap_ptr + mmap_size, kGapAlignment);
    gap_start = gap_end - gap_size;
  } else {
    gap_start = gap_end = mmap_ptr + mmap_size;
  }

  uint8_t *first = align_up(mmap_ptr, mapping_align);
  uint8_t *last = align_down(gap_start, mapping_align) - size;

  // arc4random* is not available in first stage init because /dev/urandom
  // hasn't yet been created. Don't randomize then.
  size_t n = is_first_stage_init() ? 0 : arc4random_uniform((last - first) / start_align + 1);
  uint8_t *start = first + n * start_align;
  // Unmap the extra space around the allocation.
  // Keep it mapped PROT_NONE on 64-bit targets where address space is plentiful
  // to make it harder to defeat ASLR by probing for readable memory mappings.
  munmap(mmap_ptr, start - mmap_ptr);
  munmap(start + size, gap_start - (start + size));
  if (gap_end != mmap_ptr + mmap_size) {
    munmap(gap_end, mmap_ptr + mmap_size - gap_end);
  }
  *out_gap_start = gap_start;
  *out_gap_size = gap_size;
  return start;
}

// Reserve a virtual address range big enough to hold all loadable
// segments of a program header table. This is done by creating a
// private anonymous mmap() with PROT_NONE.
bool ElfReader::ReserveAddressSpace(address_space_params *address_space) {
  ElfW(Addr) min_vaddr;
  load_size_ = phdr_table_get_load_size(phdr_table_, phdr_num_, &min_vaddr);
  if (load_size_ == 0) {
    DL_ERR("\"%s\" has no loadable segments", name_.c_str());
    return false;
  }

  if (should_use_16kib_app_compat_) {
    // Reserve additional space for aligning the permission boundary in compat loading
    // Up to kPageSize-kCompatPageSize additional space is needed, but reservation
    // is done with mmap which gives kPageSize multiple-sized reservations.
    load_size_ += page_size();
  }

  uint8_t *addr = reinterpret_cast<uint8_t *>(min_vaddr);
  void *start;

  if (load_size_ > address_space->reserved_size) {
    if (address_space->must_use_address) {
      DL_ERR("reserved address space %zd smaller than %zd bytes needed for \"%s\"",
             load_size_ - address_space->reserved_size, load_size_, name_.c_str());
      return false;
    }
    size_t start_alignment = page_size();
    if (get_transparent_hugepages_supported() && android_api >= 31) {
      size_t maximum_alignment = phdr_table_get_maximum_alignment(phdr_table_, phdr_num_);
      // Limit alignment to PMD size as other alignments reduce the number of
      // bits available for ASLR for no benefit.
      start_alignment = maximum_alignment == kPmdSize ? kPmdSize : page_size();
    }
    start = ReserveWithAlignmentPadding(load_size_, kLibraryAlignment, start_alignment, &gap_start_, &gap_size_);
    if (start == nullptr) {
      DL_ERR("couldn't reserve %zd bytes of address space for \"%s\"", load_size_, name_.c_str());
      return false;
    }
  } else {
    start = address_space->start_addr;
    gap_start_ = nullptr;
    gap_size_ = 0;
    mapped_by_caller_ = true;

    // Update the reserved address space to subtract the space used by this
    // library.
    address_space->start_addr = reinterpret_cast<uint8_t *>(address_space->start_addr) + load_size_;
    address_space->reserved_size -= load_size_;
  }

  load_start_ = start;
  load_bias_ = reinterpret_cast<uint8_t *>(start) - addr;
  if (should_use_16kib_app_compat_) {
    // In compat mode make the initial mapping RW since the ELF contents will be read
    // into it; instead of mapped over it.
    mprotect(reinterpret_cast<void *>(start), load_size_, PROT_READ | PROT_WRITE);
  }
  return true;
}

bool page_size_migration_supported() {
  static bool pgsize_migration_enabled = []() {
    std::string enabled;
    if (!ReadFileToString("/sys/kernel/mm/pgsize_migration/enabled", &enabled)) {
      return false;
    }
    return enabled.find("1") != std::string::npos;
  }();
  return pgsize_migration_enabled;
}

// Find the ELF note of type NT_ANDROID_TYPE_PAD_SEGMENT and check that the desc value is 1.
bool ElfReader::ReadPadSegmentNote() {
  if (!page_size_migration_supported()) {
    // Don't attempt to read the note, since segment extension isn't
    // supported; but return true so that loading can continue normally.
    return true;
  }

  // The ELF can have multiple PT_NOTE's, check them all
  for (size_t i = 0; i < phdr_num_; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table_[i];

    if (phdr->p_type != PT_NOTE) {
      continue;
    }

    // Some obfuscated ELFs may contain "empty" PT_NOTE program headers that don't
    // point to any part of the ELF (p_memsz == 0). Skip these since there is
    // nothing to decode. See: b/324468126
    if (phdr->p_memsz == 0) {
      continue;
    }

    // If the PT_NOTE extends beyond the file. The ELF is doing something
    // strange -- obfuscation, embedding hidden loaders, ...
    //
    // It doesn't contain the pad_segment note. Skip it to avoid SIGBUS
    // by accesses beyond the file.
    off64_t note_end_off = file_offset_ + phdr->p_offset + phdr->p_filesz;
    if (note_end_off > file_size_) {
      continue;
    }

    // note_fragment is scoped to within the loop so that there is
    // at most 1 PT_NOTE mapped at anytime during this search.
    MappedFileFragment note_fragment;
    if (!note_fragment.Map(fd_, file_offset_, phdr->p_offset, phdr->p_memsz)) {
      DL_ERR("\"%s\": PT_NOTE mmap(nullptr, %p, PROT_READ, MAP_PRIVATE, %d, %p) failed: %m", name_.c_str(),
             reinterpret_cast<void *>(phdr->p_memsz), fd_,
             reinterpret_cast<void *>(PAGE_START(file_offset_ + phdr->p_offset)));
      return false;
    }

    const ElfW(Nhdr) *note_hdr = nullptr;
    const char *note_desc = nullptr;
    if (!__get_elf_note(NT_ANDROID_TYPE_PAD_SEGMENT, "Android", reinterpret_cast<ElfW(Addr)>(note_fragment.data()),
                        phdr, &note_hdr, &note_desc)) {
      continue;
    }

    if (note_hdr->n_descsz != sizeof(ElfW(Word))) {
      DL_ERR("\"%s\" NT_ANDROID_TYPE_PAD_SEGMENT note has unexpected n_descsz: %u", name_.c_str(),
             reinterpret_cast<unsigned int>(note_hdr->n_descsz));
      return false;
    }

    // 1 == enabled, 0 == disabled
    should_pad_segments_ = *reinterpret_cast<const ElfW(Word) *>(note_desc) == 1;
    return true;
  }

  return true;
}

static inline void _extend_load_segment_vma(const ElfW(Phdr) * phdr_table, size_t phdr_count, size_t phdr_idx,
                                            ElfW(Addr) * p_memsz, ElfW(Addr) * p_filesz, bool should_pad_segments,
                                            bool should_use_16kib_app_compat) {
  // NOTE: Segment extension is only applicable where the ELF's max-page-size > runtime page size;
  // to save kernel VMA slab memory. 16KiB compat mode is the exact opposite scenario.
  if (should_use_16kib_app_compat) {
    return;
  }

  const ElfW(Phdr) *phdr = &phdr_table[phdr_idx];
  const ElfW(Phdr) *next = nullptr;
  size_t next_idx = phdr_idx + 1;

  // Don't do segment extension for p_align > 64KiB, such ELFs already existed in the
  // field e.g. 2MiB p_align for THPs and are relatively small in number.
  //
  // The kernel can only represent padding for p_align up to 64KiB. This is because
  // the kernel uses 4 available bits in the vm_area_struct to represent padding
  // extent; and so cannot enable mitigations to avoid breaking app compatibility for
  // p_aligns > 64KiB.
  //
  // Don't perform segment extension on these to avoid app compatibility issues.
  if (phdr->p_align <= page_size() || phdr->p_align > 64 * 1024 || !should_pad_segments) {
    return;
  }

  if (next_idx < phdr_count && phdr_table[next_idx].p_type == PT_LOAD) {
    next = &phdr_table[next_idx];
  }

  // If this is the last LOAD segment, no extension is needed
  if (!next || *p_memsz != *p_filesz) {
    return;
  }

  ElfW(Addr) next_start = PAGE_START(next->p_vaddr);
  ElfW(Addr) curr_end = PAGE_END(phdr->p_vaddr + *p_memsz);

  // If adjacent segment mappings overlap, no extension is needed.
  if (curr_end >= next_start) {
    return;
  }

  // Extend the LOAD segment mapping to be contiguous with that of
  // the next LOAD segment.
  ElfW(Addr) extend = next_start - curr_end;
  *p_memsz += extend;
  *p_filesz += extend;
}

bool ElfReader::MapSegment(size_t seg_idx, size_t len) {
  const ElfW(Phdr) *phdr = &phdr_table_[seg_idx];

  void *start = reinterpret_cast<void *>(PAGE_START(phdr->p_vaddr + load_bias_));

  // The ELF could be being loaded directly from a zipped APK,
  // the zip offset must be added to find the segment offset.
  const ElfW(Addr) offset = file_offset_ + PAGE_START(phdr->p_offset);

  int prot = PFLAGS_TO_PROT(phdr->p_flags);

  void *seg_addr = mmap64(start, len, prot, MAP_FIXED | MAP_PRIVATE, fd_, offset);

  if (seg_addr == MAP_FAILED) {
    DL_ERR("couldn't map \"%s\" segment %zd: %m", name_.c_str(), seg_idx);
    return false;
  }

  // Mark segments as huge page eligible if they meet the requirements
  if ((phdr->p_flags & PF_X) && phdr->p_align == kPmdSize && get_transparent_hugepages_supported()) {
    madvise(seg_addr, len, MADV_HUGEPAGE);
  }

  return true;
}

void ElfReader::ZeroFillSegment(const ElfW(Phdr) * phdr) {
  // NOTE: In 16KiB app compat mode, the ELF mapping is anonymous, meaning that
  // RW segments are COW-ed from the kernel's zero page. So there is no need to
  // explicitly zero-fill until the last page's limit.
  if (should_use_16kib_app_compat_) {
    return;
  }

  ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
  uint64_t unextended_seg_file_end = seg_start + phdr->p_filesz;

  // If the segment is writable, and does not end on a page boundary,
  // zero-fill it until the page limit.
  //
  // Do not attempt to zero the extended region past the first partial page,
  // since doing so may:
  //   1) Result in a SIGBUS, as the region is not backed by the underlying
  //      file.
  //   2) Break the COW backing, faulting in new anon pages for a region
  //      that will not be used.
  if ((phdr->p_flags & PF_W) != 0 && page_offset(unextended_seg_file_end) > 0) {
    memset(reinterpret_cast<void *>(unextended_seg_file_end), 0, page_size() - page_offset(unextended_seg_file_end));
  }
}

void ElfReader::DropPaddingPages(const ElfW(Phdr) * phdr, uint64_t seg_file_end) {
  // NOTE: Padding pages are only applicable where the ELF's max-page-size > runtime page size;
  // 16KiB compat mode is the exact opposite scenario.
  if (should_use_16kib_app_compat_) {
    return;
  }

  ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
  uint64_t unextended_seg_file_end = seg_start + phdr->p_filesz;

  uint64_t pad_start = PAGE_END(unextended_seg_file_end);
  uint64_t pad_end = PAGE_END(seg_file_end);
  CHECK(pad_start <= pad_end);

  uint64_t pad_len = pad_end - pad_start;
  if (pad_len == 0 || !page_size_migration_supported()) {
    return;
  }

  // Pages may be brought in due to readahead.
  // Drop the padding (zero) pages, to avoid reclaim work later.
  //
  // NOTE: The madvise() here is special, as it also serves to hint to the
  // kernel the portion of the LOAD segment that is padding.
  //
  // See: [1] https://android-review.googlesource.com/c/kernel/common/+/3032411
  //      [2] https://android-review.googlesource.com/c/kernel/common/+/3048835
  if (madvise(reinterpret_cast<void *>(pad_start), pad_len, MADV_DONTNEED)) {
    DL_WARN("\"%s\": madvise(0x%" PRIx64 ", 0x%" PRIx64 ", MADV_DONTNEED) failed: %m", name_.c_str(), pad_start,
            pad_len);
  }
}

bool ElfReader::MapBssSection(const ElfW(Phdr) * phdr, ElfW(Addr) seg_page_end, ElfW(Addr) seg_file_end) {
  // NOTE: We do not need to handle .bss in 16KiB compat mode since the mapping
  // reservation is anonymous and RW to begin with.
  if (should_use_16kib_app_compat_) {
    return true;
  }

  // seg_file_end is now the first page address after the file content.
  seg_file_end = PAGE_END(seg_file_end);

  if (seg_page_end <= seg_file_end) {
    return true;
  }

  // If seg_page_end is larger than seg_file_end, we need to zero
  // anything between them. This is done by using a private anonymous
  // map for all extra pages
  size_t zeromap_size = seg_page_end - seg_file_end;
  void *zeromap = mmap(reinterpret_cast<void *>(seg_file_end), zeromap_size, PFLAGS_TO_PROT(phdr->p_flags),
                       MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (zeromap == MAP_FAILED) {
    DL_ERR("couldn't map .bss section for \"%s\": %m", name_.c_str());
    return false;
  }

  // Set the VMA name using prctl
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, zeromap, zeromap_size, ".bss");

  return true;
}

bool ElfReader::LoadSegments() {
  // NOTE: The compat(legacy) page size (4096) must be used when aligning
  // the 4KiB segments for loading in compat mode. The larger 16KiB page size
  // will lead to overwriting adjacent segments since the ELF's segment(s)
  // are not 16KiB aligned.
  size_t seg_align = should_use_16kib_app_compat_ ? kCompatPageSize : page_size();

  size_t min_palign = phdr_table_get_minimum_alignment(phdr_table_, phdr_num_);
  // Only enforce this on 16 KB systems with app compat disabled.
  // Apps may rely on undefined behavior here on 4 KB systems,
  // which is the norm before this change is introduced
  if (page_size() >= 16384 && min_palign < page_size() && !should_use_16kib_app_compat_) {
    DL_ERR("\"%s\" program alignment (%zu) cannot be smaller than system page size (%zu)", name_.c_str(), min_palign,
           page_size());
    return false;
  }

  if (!Setup16KiBAppCompat()) {
    DL_ERR("\"%s\" failed to setup 16KiB App Compat", name_.c_str());
    return false;
  }

  for (size_t i = 0; i < phdr_num_; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table_[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    ElfW(Addr) p_memsz = phdr->p_memsz;
    ElfW(Addr) p_filesz = phdr->p_filesz;
    _extend_load_segment_vma(phdr_table_, phdr_num_, i, &p_memsz, &p_filesz, should_pad_segments_,
                             should_use_16kib_app_compat_);

    // Segment addresses in memory.
    ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
    ElfW(Addr) seg_end = seg_start + p_memsz;

    ElfW(Addr) seg_page_end = align_up(seg_end, seg_align);

    ElfW(Addr) seg_file_end = seg_start + p_filesz;

    // File offsets.
    ElfW(Addr) file_start = phdr->p_offset;
    ElfW(Addr) file_end = file_start + p_filesz;

    ElfW(Addr) file_page_start = align_down(file_start, seg_align);
    ElfW(Addr) file_length = file_end - file_page_start;

    if (file_size_ <= 0) {
      DL_ERR("\"%s\" invalid file size: %" PRId64, name_.c_str(), file_size_);
      return false;
    }

    if (file_start + phdr->p_filesz > static_cast<size_t>(file_size_)) {
      DL_ERR("invalid ELF file \"%s\" load segment[%zd]:"
             " p_offset (%p) + p_filesz (%p) ( = %p) past end of file (0x%" PRIx64 ")",
             name_.c_str(), i, reinterpret_cast<void *>(phdr->p_offset), reinterpret_cast<void *>(phdr->p_filesz),
             reinterpret_cast<void *>(file_start + phdr->p_filesz), file_size_);
      return false;
    }

    if (file_length != 0) {
      int prot = PFLAGS_TO_PROT(phdr->p_flags);
      if ((prot & (PROT_EXEC | PROT_WRITE)) == (PROT_EXEC | PROT_WRITE)) {
        // W + E PT_LOAD segments are not allowed in O.
        if (android_api >= 26) {
          DL_ERR_AND_LOG("\"%s\": W+E load segments are not allowed", name_.c_str());
          return false;
        }
        DL_WARN_documented_change(26, "writable-and-executable-segments-enforced-for-api-level-26",
                                  "\"%s\" has load segments that are both writable and executable", name_.c_str());
        add_dlwarning(name_.c_str(), "W+E load segments");
      }

      // Pass the file_length, since it may have been extended by _extend_load_segment_vma().
      if (should_use_16kib_app_compat_) {
        if (!CompatMapSegment(i, file_length)) {
          return false;
        }
      } else {
        if (!MapSegment(i, file_length)) {
          return false;
        }
      }
    }

    ZeroFillSegment(phdr);

    DropPaddingPages(phdr, seg_file_end);

    if (!MapBssSection(phdr, seg_page_end, seg_file_end)) {
      return false;
    }
  }
  return true;
}

/* Used internally. Used to set the protection bits of all loaded segments
 * with optional extra flags (i.e. really PROT_WRITE). Used by
 * phdr_table_protect_segments and phdr_table_unprotect_segments.
 */
static int _phdr_table_set_load_prot(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                     int extra_prot_flags, bool should_pad_segments, bool should_use_16kib_app_compat) {
  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) *phdr = &phdr_table[i];

    if (phdr->p_type != PT_LOAD || (phdr->p_flags & PF_W) != 0) {
      continue;
    }

    ElfW(Addr) p_memsz = phdr->p_memsz;
    ElfW(Addr) p_filesz = phdr->p_filesz;
    _extend_load_segment_vma(phdr_table, phdr_count, i, &p_memsz, &p_filesz, should_pad_segments,
                             should_use_16kib_app_compat);

    ElfW(Addr) seg_page_start = PAGE_START(phdr->p_vaddr + load_bias);
    ElfW(Addr) seg_page_end = PAGE_END(phdr->p_vaddr + p_memsz + load_bias);

    int prot = PFLAGS_TO_PROT(phdr->p_flags) | extra_prot_flags;
    if ((prot & PROT_WRITE) != 0) {
      // make sure we're never simultaneously writable / executable
      prot &= ~PROT_EXEC;
    }
#if defined(__aarch64__)
    if ((prot & PROT_EXEC) == 0) {
      // Though it is not specified don't add PROT_BTI if segment is not
      // executable.
      prot &= ~PROT_BTI;
    }
#endif

    int ret = mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_end - seg_page_start, prot);
    if (ret < 0) {
      return -1;
    }
  }
  return 0;
}

/* Restore the original protection modes for all loadable segments.
 * You should only call this after phdr_table_unprotect_segments and
 * applying all relocations.
 *
 * AArch64: also called from linker_main and ElfReader::Load to apply
 *     PROT_BTI for loaded main so and other so-s.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 *   should_pad_segments -> Are segments extended to avoid gaps in the memory map
 *   should_use_16kib_app_compat -> Is the ELF being loaded in 16KiB app compat mode.
 *   prop        -> GnuPropertySection or nullptr
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_protect_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                bool should_pad_segments, bool should_use_16kib_app_compat,
                                const GnuPropertySection *prop __unused) {
  int prot = 0;
#if defined(__aarch64__)
  if ((prop != nullptr) && prop->IsBTICompatible()) {
    prot |= PROT_BTI;
  }
#endif
  return _phdr_table_set_load_prot(phdr_table, phdr_count, load_bias, prot, should_pad_segments,
                                   should_use_16kib_app_compat);
}

static bool segment_needs_memtag_globals_remapping(const ElfW(Phdr) * phdr) {
  // For now, MTE globals is only supported on writeable data segments.
  return phdr->p_type == PT_LOAD && !(phdr->p_flags & PF_X) && (phdr->p_flags & PF_W);
}

/* When MTE globals are requested by the binary, and when the hardware supports
 * it, remap the executable's PT_LOAD data pages to have PROT_MTE.
 *
 * Returns 0 on success, -1 on failure (error code in errno).
 */
int remap_memtag_globals_segments(const ElfW(Phdr) * phdr_table __unused, size_t phdr_count __unused,
                                  ElfW(Addr) load_bias __unused) {
#if defined(__aarch64__)
  for (const ElfW(Phdr) *phdr = phdr_table; phdr < phdr_table + phdr_count; phdr++) {
    if (!segment_needs_memtag_globals_remapping(phdr)) {
      continue;
    }

    uintptr_t seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    uintptr_t seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
    size_t seg_page_aligned_size = seg_page_end - seg_page_start;

    int prot = PFLAGS_TO_PROT(phdr->p_flags);
    // For anonymous private mappings, it may be possible to simply mprotect()
    // the PROT_MTE flag over the top. For file-based mappings, this will fail,
    // and we'll need to fall back. We also allow PROT_WRITE here to allow
    // writing memory tags (in `soinfo::tag_globals()`), and set these sections
    // back to read-only after tags are applied (similar to RELRO).
    prot |= PROT_MTE;
    if (mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_aligned_size, prot | PROT_WRITE) == 0) {
      continue;
    }

    void *mapping_copy =
      mmap(nullptr, seg_page_aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(mapping_copy, reinterpret_cast<void *>(seg_page_start), seg_page_aligned_size);

    void *seg_addr = mmap(reinterpret_cast<void *>(seg_page_start), seg_page_aligned_size, prot | PROT_WRITE,
                          MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (seg_addr == MAP_FAILED)
      return -1;

    memcpy(seg_addr, mapping_copy, seg_page_aligned_size);
    munmap(mapping_copy, seg_page_aligned_size);
  }
#endif // defined(__aarch64__)
  return 0;
}

void protect_memtag_globals_ro_segments(const ElfW(Phdr) * phdr_table __unused, size_t phdr_count __unused,
                                        ElfW(Addr) load_bias __unused) {
#if defined(__aarch64__)
  for (const ElfW(Phdr) *phdr = phdr_table; phdr < phdr_table + phdr_count; phdr++) {
    int prot = PFLAGS_TO_PROT(phdr->p_flags);
    if (!segment_needs_memtag_globals_remapping(phdr) || (prot & PROT_WRITE)) {
      continue;
    }

    prot |= PROT_MTE;

    uintptr_t seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    uintptr_t seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
    size_t seg_page_aligned_size = seg_page_end - seg_page_start;
    mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_aligned_size, prot);
  }
#endif // defined(__aarch64__)
}

void name_memtag_globals_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                  const char *soname, std::list<std::string> *vma_names) {
  for (const ElfW(Phdr) *phdr = phdr_table; phdr < phdr_table + phdr_count; phdr++) {
    if (!segment_needs_memtag_globals_remapping(phdr)) {
      continue;
    }

    uintptr_t seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    uintptr_t seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
    size_t seg_page_aligned_size = seg_page_end - seg_page_start;

    // For file-based mappings that we're now forcing to be anonymous mappings, set the VMA name to
    // make debugging easier.
    // Once we are targeting only devices that run kernel 5.10 or newer (and thus include
    // https://android-review.git.corp.google.com/c/kernel/common/+/1934723 which causes the
    // VMA_ANON_NAME to be copied into the kernel), we can get rid of the storage here.
    // For now, that is not the case:
    // https://source.android.com/docs/core/architecture/kernel/android-common#compatibility-matrix
    constexpr int kVmaNameLimit = 80;
    std::string &vma_name = vma_names->emplace_back(kVmaNameLimit, '\0');
    int full_vma_length = snprintf(vma_name.data(), kVmaNameLimit, "mt:%s+%" PRIxPTR, soname,
                                   (uintptr_t)(PAGE_START(phdr->p_vaddr) + /* include the null terminator */ 1));

    // There's an upper limit of 80 characters, including the null terminator, in the anonymous VMA
    // name. If we run over that limit, we end up truncating the segment offset and parts of the
    // DSO's name, starting on the right hand side of the basename. Because the basename is the most
    // important thing, chop off the soname from the left hand side first.
    //
    // Example (with '#' as the null terminator):
    //   - "mt:/data/nativetest64/bionic-unit-tests/bionic-loader-test-libs/libdlext_test.so+e000#"
    //     is a `full_vma_length` == 86.
    //
    // We need to left-truncate (86 - 80) 6 characters from the soname, plus the
    // `vma_truncation_prefix`, so 9 characters total.
    if (full_vma_length > kVmaNameLimit) {
      const char vma_truncation_prefix[] = "...";
      int soname_truncated_bytes = full_vma_length - kVmaNameLimit + sizeof(vma_truncation_prefix) - 1;
      snprintf(vma_name.data(), kVmaNameLimit, "mt:%s%s+%" PRIxPTR, vma_truncation_prefix,
               soname + soname_truncated_bytes, (uintptr_t)PAGE_START(phdr->p_vaddr));
    }
    if (prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, reinterpret_cast<void *>(seg_page_start), seg_page_aligned_size,
              vma_name.data()) != 0) {
      DL_WARN("Failed to rename memtag global segment: %m");
    }
  }
}

/* Change the protection of all loaded segments in memory to writable.
 * This is useful before performing relocations. Once completed, you
 * will have to call phdr_table_protect_segments to restore the original
 * protection flags on all segments.
 *
 * Note that some writable segments can also have their content turned
 * to read-only by calling phdr_table_protect_gnu_relro. This is no
 * performed here.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 *   should_pad_segments -> Are segments extended to avoid gaps in the memory map
 *   should_use_16kib_app_compat -> Is the ELF being loaded in 16KiB app compat mode.
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_unprotect_segments(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                  bool should_pad_segments, bool should_use_16kib_app_compat) {
  return _phdr_table_set_load_prot(phdr_table, phdr_count, load_bias, PROT_WRITE, should_pad_segments,
                                   should_use_16kib_app_compat);
}

static inline void _extend_gnu_relro_prot_end(const ElfW(Phdr) * relro_phdr, const ElfW(Phdr) * phdr_table,
                                              size_t phdr_count, ElfW(Addr) load_bias, ElfW(Addr) * seg_page_end,
                                              bool should_pad_segments, bool should_use_16kib_app_compat) {
  // Find the index and phdr of the LOAD containing the GNU_RELRO segment
  for (size_t index = 0; index < phdr_count; ++index) {
    const ElfW(Phdr) *phdr = &phdr_table[index];

    if (phdr->p_type == PT_LOAD && phdr->p_vaddr == relro_phdr->p_vaddr) {
      // If the PT_GNU_RELRO mem size is not at least as large as the corresponding
      // LOAD segment mem size, we need to protect only a partial region of the
      // LOAD segment and therefore cannot avoid a VMA split.
      //
      // Note: Don't check the page-aligned mem sizes since the extended protection
      // may incorrectly write protect non-relocation data.
      //
      // Example:
      //
      //               |---- 3K ----|-- 1K --|---- 3K ---- |-- 1K --|
      //       ----------------------------------------------------------------
      //               |            |        |             |        |
      //        SEG X  |     RO     |   RO   |     RW      |        |   SEG Y
      //               |            |        |             |        |
      //       ----------------------------------------------------------------
      //                            |        |             |
      //                            |        |             |
      //                            |        |             |
      //                    relro_vaddr   relro_vaddr   relro_vaddr
      //                    (load_vaddr)       +            +
      //                                  relro_memsz   load_memsz
      //
      //       ----------------------------------------------------------------
      //               |         PAGE        |         PAGE         |
      //       ----------------------------------------------------------------
      //                                     |       Potential      |
      //                                     |----- Extended RO ----|
      //                                     |      Protection      |
      //
      // If the check below uses  page aligned mem sizes it will cause incorrect write
      // protection of the 3K RW part of the LOAD segment containing the GNU_RELRO.
      if (relro_phdr->p_memsz < phdr->p_memsz) {
        return;
      }

      ElfW(Addr) p_memsz = phdr->p_memsz;
      ElfW(Addr) p_filesz = phdr->p_filesz;

      // Attempt extending the VMA (mprotect range). Without extending the range,
      // mprotect will only RO protect a part of the extended RW LOAD segment, which
      // will leave an extra split RW VMA (the gap).
      _extend_load_segment_vma(phdr_table, phdr_count, index, &p_memsz, &p_filesz, should_pad_segments,
                               should_use_16kib_app_compat);

      *seg_page_end = PAGE_END(phdr->p_vaddr + p_memsz + load_bias);
      return;
    }
  }
}

/* Used internally by phdr_table_protect_gnu_relro and
 * phdr_table_unprotect_gnu_relro.
 */
static int _phdr_table_set_gnu_relro_prot(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                          int prot_flags, bool should_pad_segments, bool should_use_16kib_app_compat) {
  const ElfW(Phdr) *phdr = phdr_table;
  const ElfW(Phdr) *phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_GNU_RELRO) {
      continue;
    }

    // Tricky: what happens when the relro segment does not start
    // or end at page boundaries? We're going to be over-protective
    // here and put every page touched by the segment as read-only.

    // This seems to match Ian Lance Taylor's description of the
    // feature at http://www.airs.com/blog/archives/189.

    //    Extract:
    //       Note that the current dynamic linker code will only work
    //       correctly if the PT_GNU_RELRO segment starts on a page
    //       boundary. This is because the dynamic linker rounds the
    //       p_vaddr field down to the previous page boundary. If
    //       there is anything on the page which should not be read-only,
    //       the program is likely to fail at runtime. So in effect the
    //       linker must only emit a PT_GNU_RELRO segment if it ensures
    //       that it starts on a page boundary.
    ElfW(Addr) seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    ElfW(Addr) seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
    _extend_gnu_relro_prot_end(phdr, phdr_table, phdr_count, load_bias, &seg_page_end, should_pad_segments,
                               should_use_16kib_app_compat);

    int ret = mprotect(reinterpret_cast<void *>(seg_page_start), seg_page_end - seg_page_start, prot_flags);
    if (ret < 0) {
      return -1;
    }
  }
  return 0;
}

/* Apply GNU relro protection if specified by the program header. This will
 * turn some of the pages of a writable PT_LOAD segment to read-only, as
 * specified by one or more PT_GNU_RELRO segments. This must be always
 * performed after relocations.
 *
 * The areas typically covered are .got and .data.rel.ro, these are
 * read-only from the program's POV, but contain absolute addresses
 * that need to be relocated before use.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 *   should_pad_segments -> Were segments extended to avoid gaps in the memory map
 *   should_use_16kib_app_compat -> Is the ELF being loaded in 16KiB app compat mode.
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_protect_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                 bool should_pad_segments, bool should_use_16kib_app_compat) {
  return _phdr_table_set_gnu_relro_prot(phdr_table, phdr_count, load_bias, PROT_READ, should_pad_segments,
                                        should_use_16kib_app_compat);
}

int phdr_table_unprotect_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                   bool should_pad_segments, bool should_use_16kib_app_compat) {
  return _phdr_table_set_gnu_relro_prot(phdr_table, phdr_count, load_bias, PROT_READ | PROT_WRITE, should_pad_segments,
                                        should_use_16kib_app_compat);
}

/*
 * Apply RX protection to the compat relro region of the ELF being loaded in
 * 16KiB compat mode.
 *
 * Input:
 *   start  -> start address of the compat relro region.
 *   size   -> size of the compat relro region in bytes.
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_protect_gnu_relro_16kib_compat(ElfW(Addr) start, ElfW(Addr) size) {
  return mprotect(reinterpret_cast<void *>(start), size, PROT_READ | PROT_EXEC);
}

/* Serialize the GNU relro segments to the given file descriptor. This can be
 * performed after relocations to allow another process to later share the
 * relocated segment, if it was loaded at the same address.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 *   fd          -> writable file descriptor to use
 *   file_offset -> pointer to offset into file descriptor to use/update
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_serialize_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias, int fd,
                                   size_t *file_offset) {
  const ElfW(Phdr) *phdr = phdr_table;
  const ElfW(Phdr) *phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_GNU_RELRO) {
      continue;
    }

    ElfW(Addr) seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    ElfW(Addr) seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;
    ssize_t size = seg_page_end - seg_page_start;

    ssize_t written = TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<void *>(seg_page_start), size));
    if (written != size) {
      return -1;
    }
    void *map =
      mmap(reinterpret_cast<void *>(seg_page_start), size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, *file_offset);
    if (map == MAP_FAILED) {
      return -1;
    }
    *file_offset += size;
  }
  return 0;
}

/* Where possible, replace the GNU relro segments with mappings of the given
 * file descriptor. This can be performed after relocations to allow a file
 * previously created by phdr_table_serialize_gnu_relro in another process to
 * replace the dirty relocated pages, saving memory, if it was loaded at the
 * same address. We have to compare the data before we map over it, since some
 * parts of the relro segment may not be identical due to other libraries in
 * the process being loaded at different addresses.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 *   fd          -> readable file descriptor to use
 *   file_offset -> pointer to offset into file descriptor to use/update
 * Return:
 *   0 on success, -1 on failure (error code in errno).
 */
int phdr_table_map_gnu_relro(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias, int fd,
                             size_t *file_offset) {
  // Map the file at a temporary location so we can compare its contents.
  struct stat file_stat;
  if (TEMP_FAILURE_RETRY(fstat(fd, &file_stat)) != 0) {
    return -1;
  }
  off_t file_size = file_stat.st_size;
  void *temp_mapping = nullptr;
  if (file_size > 0) {
    temp_mapping = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (temp_mapping == MAP_FAILED) {
      return -1;
    }
  }

  // Iterate over the relro segments and compare/remap the pages.
  const ElfW(Phdr) *phdr = phdr_table;
  const ElfW(Phdr) *phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_GNU_RELRO) {
      continue;
    }

    ElfW(Addr) seg_page_start = PAGE_START(phdr->p_vaddr) + load_bias;
    ElfW(Addr) seg_page_end = PAGE_END(phdr->p_vaddr + phdr->p_memsz) + load_bias;

    char *file_base = static_cast<char *>(temp_mapping) + *file_offset;
    char *mem_base = reinterpret_cast<char *>(seg_page_start);
    size_t match_offset = 0;
    size_t size = seg_page_end - seg_page_start;

    if (file_size - *file_offset < size) {
      // File is too short to compare to this segment. The contents are likely
      // different as well (it's probably for a different library version) so
      // just don't bother checking.
      break;
    }

    while (match_offset < size) {
      // Skip over dissimilar pages.
      while (match_offset < size && memcmp(mem_base + match_offset, file_base + match_offset, page_size()) != 0) {
        match_offset += page_size();
      }

      // Count similar pages.
      size_t mismatch_offset = match_offset;
      while (mismatch_offset < size &&
             memcmp(mem_base + mismatch_offset, file_base + mismatch_offset, page_size()) == 0) {
        mismatch_offset += page_size();
      }

      // Map over similar pages.
      if (mismatch_offset > match_offset) {
        void *map = mmap(mem_base + match_offset, mismatch_offset - match_offset, PROT_READ, MAP_PRIVATE | MAP_FIXED,
                         fd, *file_offset + match_offset);
        if (map == MAP_FAILED) {
          munmap(temp_mapping, file_size);
          return -1;
        }
      }

      match_offset = mismatch_offset;
    }

    // Add to the base file offset in case there are multiple relro segments.
    *file_offset += size;
  }
  munmap(temp_mapping, file_size);
  return 0;
}

#if defined(__arm__)
/* Return the address and size of the .ARM.exidx section in memory,
 * if present.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Output:
 *   arm_exidx       -> address of table in memory (null on failure).
 *   arm_exidx_count -> number of items in table (0 on failure).
 * Return:
 *   0 on success, -1 on failure (_no_ error code in errno)
 */
int phdr_table_get_arm_exidx(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                             ElfW(Addr) * *arm_exidx, size_t *arm_exidx_count) {
  const ElfW(Phdr) *phdr = phdr_table;
  const ElfW(Phdr) *phdr_limit = phdr + phdr_count;

  for (phdr = phdr_table; phdr < phdr_limit; phdr++) {
    if (phdr->p_type != PT_ARM_EXIDX) {
      continue;
    }

    *arm_exidx = reinterpret_cast<ElfW(Addr) *>(load_bias + phdr->p_vaddr);
    *arm_exidx_count = phdr->p_memsz / 8;
    return 0;
  }
  *arm_exidx = nullptr;
  *arm_exidx_count = 0;
  return -1;
}
#endif

/* Return the address and size of the ELF file's .dynamic section in memory,
 * or null if missing.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Output:
 *   dynamic       -> address of table in memory (null on failure).
 *   dynamic_flags -> protection flags for section (unset on failure)
 * Return:
 *   void
 */
void phdr_table_get_dynamic_section(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias,
                                    ElfW(Dyn) * *dynamic, ElfW(Word) * dynamic_flags) {
  *dynamic = nullptr;
  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) &phdr = phdr_table[i];
    if (phdr.p_type == PT_DYNAMIC) {
      *dynamic = reinterpret_cast<ElfW(Dyn) *>(load_bias + phdr.p_vaddr);
      if (dynamic_flags) {
        *dynamic_flags = phdr.p_flags;
      }
      return;
    }
  }
}

/* Return the program interpreter string, or nullptr if missing.
 *
 * Input:
 *   phdr_table  -> program header table
 *   phdr_count  -> number of entries in tables
 *   load_bias   -> load bias
 * Return:
 *   pointer to the program interpreter string.
 */
const char *phdr_table_get_interpreter_name(const ElfW(Phdr) * phdr_table, size_t phdr_count, ElfW(Addr) load_bias) {
  for (size_t i = 0; i < phdr_count; ++i) {
    const ElfW(Phdr) &phdr = phdr_table[i];
    if (phdr.p_type == PT_INTERP) {
      return reinterpret_cast<const char *>(load_bias + phdr.p_vaddr);
    }
  }
  return nullptr;
}

// Sets loaded_phdr_ to the address of the program header table as it appears
// in the loaded segments in memory. This is in contrast with phdr_table_,
// which is temporary and will be released before the library is relocated.
bool ElfReader::FindPhdr() {
  const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;

  // If there is a PT_PHDR, use it directly.
  for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type == PT_PHDR) {
      return CheckPhdr(load_bias_ + phdr->p_vaddr);
    }
  }

  // Otherwise, check the first loadable segment. If its file offset
  // is 0, it starts with the ELF header, and we can trivially find the
  // loaded program header from it.
  for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type == PT_LOAD) {
      if (phdr->p_offset == 0) {
        ElfW(Addr) elf_addr = load_bias_ + phdr->p_vaddr;
        const ElfW(Ehdr) *ehdr = reinterpret_cast<const ElfW(Ehdr) *>(elf_addr);
        ElfW(Addr) offset = ehdr->e_phoff;
        return CheckPhdr(reinterpret_cast<ElfW(Addr)>(ehdr) + offset);
      }
      break;
    }
  }

  DL_ERR("can't find loaded phdr for \"%s\"", name_.c_str());
  return false;
}

// Tries to find .note.gnu.property section.
// It is not considered an error if such section is missing.
bool ElfReader::FindGnuPropertySection() {
#if defined(__aarch64__)
  note_gnu_property_ = GnuPropertySection(phdr_table_, phdr_num_, load_start(), name_.c_str());
#endif
  return true;
}

// Ensures that our program header is actually within a loadable
// segment. This should help catch badly-formed ELF files that
// would cause the linker to crash later when trying to access it.
bool ElfReader::CheckPhdr(ElfW(Addr) loaded) {
  const ElfW(Phdr) *phdr_limit = phdr_table_ + phdr_num_;
  ElfW(Addr) loaded_end = loaded + (phdr_num_ * sizeof(ElfW(Phdr)));
  for (const ElfW(Phdr) *phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type != PT_LOAD) {
      continue;
    }
    ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
    ElfW(Addr) seg_end = phdr->p_filesz + seg_start;
    if (seg_start <= loaded && loaded_end <= seg_end) {
      loaded_phdr_ = reinterpret_cast<const ElfW(Phdr) *>(loaded);
      return true;
    }
  }
  DL_ERR("\"%s\" loaded phdr %p not in loadable segment", name_.c_str(), reinterpret_cast<void *>(loaded));
  return false;
}
} // namespace fakelinker