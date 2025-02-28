
#include "linker_relocate.h"

#include <elf_reader.h>

#include "linker_globals.h"
#include "linker_relocs.h"
#include "linker_sleb128.h"
#include "linker_soinfo.h"

enum class RelocMode {
  // Fast path for JUMP_SLOT relocations.
  JumpTable,
  // Fast path for typical relocations: ABSOLUTE, GLOB_DAT, or RELATIVE.
  Typical,
  // Handle all relocation types, relocations in text sections, and statistics/tracing.
  General,
};

struct linker_stats_t {
  int count[kRelocMax];
};

static linker_stats_t linker_stats;

void count_relocation(RelocationKind kind) { ++linker_stats.count[kind]; }

class Relocator {
public:
  Relocator(const VersionTracker &version_tracker, const SymbolLookupList &lookup_list) :
      version_tracker(version_tracker), lookup_list(lookup_list) {}

  soinfo *si = nullptr;
  const char *si_strtab = nullptr;
  size_t si_strtab_size = 0;
  ElfW(Sym) *si_symtab = nullptr;

  const VersionTracker &version_tracker;
  const SymbolLookupList &lookup_list;

  // Cache key
  ElfW(Word) cache_sym_val = 0;
  // Cache value
  const ElfW(Sym) *cache_sym = nullptr;
  soinfo *cache_si = nullptr;

  std::vector<TlsDynamicResolverArg> *tlsdesc_args;
  std::vector<std::pair<TlsDescriptor *, size_t>> deferred_tlsdesc_relocs;
  size_t tls_tp_base = 0;

  __attribute__((always_inline)) const char *get_string(ElfW(Word) index) {
    if (__predict_false(index >= si_strtab_size)) {
      async_safe_fatal("%s: strtab out of bounds error; STRSZ=%zd, name=%d", si->realpath(), si_strtab_size, index);
    }
    return si_strtab + index;
  }
};

template <bool DoLogging>
__attribute__((always_inline)) static inline bool
lookup_symbol(Relocator &relocator, uint32_t r_sym, const char *sym_name, soinfo **found_in, const ElfW(Sym) * *sym) {
  if (r_sym == relocator.cache_sym_val) {
    *found_in = relocator.cache_si;
    *sym = relocator.cache_sym;
    count_relocation_if<DoLogging>(kRelocSymbolCached);
  } else {
    const version_info *vi = nullptr;
    if (!relocator.si->lookup_version_info(relocator.version_tracker, r_sym, sym_name, &vi)) {
      return false;
    }

    soinfo *local_found_in = nullptr;
    const ElfW(Sym) *local_sym = soinfo_do_lookup(sym_name, vi, &local_found_in, relocator.lookup_list);

    relocator.cache_sym_val = r_sym;
    relocator.cache_si = local_found_in;
    relocator.cache_sym = local_sym;
    *found_in = local_found_in;
    *sym = local_sym;
  }

  if (*sym == nullptr) {
    if (ELF_ST_BIND(relocator.si_symtab[r_sym].st_info) != STB_WEAK) {
      LOGE("cannot locate symbol \"%s\" referenced by \"%s\"...", sym_name, relocator.si->realpath());
      return false;
    }
  }

  count_relocation_if<DoLogging>(kRelocSymbol);
  return true;
}

static bool process_relocation_general(Relocator &relocator, const rel_t &reloc);

template <RelocMode Mode>
__attribute__((always_inline)) static bool process_relocation_impl(Relocator &relocator, const rel_t &reloc) {
  constexpr bool IsGeneral = Mode == RelocMode::General;

  void *const rel_target =
    reinterpret_cast<void *>(relocator.si->apply_memtag_if_mte_globals(reloc.r_offset + relocator.si->load_bias()));
  const uint32_t r_type = R_TYPE(reloc.r_info);
  const uint32_t r_sym = R_SYM(reloc.r_info);

  soinfo *found_in = nullptr;
  const ElfW(Sym) *sym = nullptr;
  const char *sym_name = nullptr;
  ElfW(Addr) sym_addr = 0;

  if (r_sym != 0) {
    sym_name = relocator.get_string(relocator.si_symtab[r_sym].st_name);
  }

  // While relocating a DSO with text relocations (obsolete and 32-bit only), the .text segment is
  // writable (but not executable). To call an ifunc, temporarily remap the segment as executable
  // (but not writable). Then switch it back to continue applying relocations in the segment.
#if defined(__LP64__)
  const bool handle_text_relocs = false;
  auto protect_segments = []() {
    return true;
  };
  auto unprotect_segments = []() {
    return true;
  };
#else
  const bool handle_text_relocs = IsGeneral && relocator.si->has_text_relocations();
  auto protect_segments = [&]() {
    // Make .text executable.
    if (fakelinker::phdr_table_protect_segments(relocator.si->phdr(), relocator.si->phnum(), relocator.si->load_bias(),
                                                relocator.si->should_pad_segments(),
                                                relocator.si->should_use_16kib_app_compat()) < 0) {
      LOGE("can't protect segments for \"%s\": %m", relocator.si->realpath());
      return false;
    }
    return true;
  };
  auto unprotect_segments = [&]() {
    // Make .text writable.
    if (fakelinker::phdr_table_unprotect_segments(relocator.si->phdr(), relocator.si->phnum(),
                                                  relocator.si->load_bias(), relocator.si->should_pad_segments(),
                                                  relocator.si->should_use_16kib_app_compat()) < 0) {
      LOGE("can't unprotect loadable segments for \"%s\": %m", relocator.si->realpath());
      return false;
    }
    return true;
  };
#endif

  // Skip symbol lookup for R_GENERIC_NONE relocations.
  if (__predict_false(r_type == R_GENERIC_NONE)) {
    LOGD("RELO NONE");
    return true;
  }

#if defined(USE_RELA)
  auto get_addend_rel = [&]() -> ElfW(Addr) {
    return reloc.r_addend;
  };
  auto get_addend_norel = [&]() -> ElfW(Addr) {
    return reloc.r_addend;
  };
#else
  auto get_addend_rel = [&]() -> ElfW(Addr) {
    return *static_cast<ElfW(Addr) *>(rel_target);
  };
  auto get_addend_norel = [&]() -> ElfW(Addr) {
    return 0;
  };
#endif

  if (!IsGeneral && __predict_false(is_tls_reloc(r_type))) {
    // Always process TLS relocations using the slow code path, so that STB_LOCAL symbols are
    // diagnosed, and ifunc processing is skipped.
    return process_relocation_general(relocator, reloc);
  }

  if (IsGeneral && is_tls_reloc(r_type)) {
    if (r_sym == 0) {
      // By convention in ld.bfd and lld, an omitted symbol on a TLS relocation
      // is a reference to the current module.
      found_in = relocator.si;
    } else if (ELF_ST_BIND(relocator.si_symtab[r_sym].st_info) == STB_LOCAL) {
      // In certain situations, the Gold linker accesses a TLS symbol using a
      // relocation to an STB_LOCAL symbol in .dynsym of either STT_SECTION or
      // STT_TLS type. Bionic doesn't support these relocations, so issue an
      // error. References:
      //  - https://groups.google.com/d/topic/generic-abi/dJ4_Y78aQ2M/discussion
      //  - https://sourceware.org/bugzilla/show_bug.cgi?id=17699
      sym = &relocator.si_symtab[r_sym];
      auto sym_type = ELF_ST_TYPE(sym->st_info);
      if (sym_type == STT_SECTION) {
        LOGE("unexpected TLS reference to local section in \"%s\": sym type %d, rel type %u", relocator.si->realpath(),
             sym_type, r_type);
      } else {
        LOGE("unexpected TLS reference to local symbol \"%s\" in \"%s\": sym type %d, rel type %u", sym_name,
             relocator.si->realpath(), sym_type, r_type);
      }
      return false;
    } else if (!lookup_symbol<IsGeneral>(relocator, r_sym, sym_name, &found_in, &sym)) {
      return false;
    }
    if (found_in != nullptr && found_in->get_tls() == nullptr) {
      // sym_name can be nullptr if r_sym is 0. A linker should never output an ELF file like this.
      LOGE("TLS relocation refers to symbol \"%s\" in solib \"%s\" with no TLS segment", sym_name,
           found_in->realpath());
      return false;
    }
    if (sym != nullptr) {
      if (ELF_ST_TYPE(sym->st_info) != STT_TLS) {
        // A toolchain should never output a relocation like this.
        LOGE("reference to non-TLS symbol \"%s\" from TLS relocation in \"%s\"", sym_name, relocator.si->realpath());
        return false;
      }
      sym_addr = sym->st_value;
    }
  } else {
    if (r_sym == 0) {
      // Do nothing.
    } else {
      if (!lookup_symbol<IsGeneral>(relocator, r_sym, sym_name, &found_in, &sym)) {
        return false;
      }
      if (sym != nullptr) {
        const bool should_protect_segments =
          handle_text_relocs && found_in == relocator.si && ELF_ST_TYPE(sym->st_info) == STT_GNU_IFUNC;
        if (should_protect_segments && !protect_segments()) {
          return false;
        }
        sym_addr = found_in->resolve_symbol_address(sym);
        if (should_protect_segments && !unprotect_segments()) {
          return false;
        }
      } else if constexpr (IsGeneral) {
        // A weak reference to an undefined symbol. We typically use a zero symbol address, but
        // use the relocation base for PC-relative relocations, so that the value written is zero.
        switch (r_type) {
#if defined(__x86_64__)
        case R_X86_64_PC32:
          sym_addr = reinterpret_cast<ElfW(Addr)>(rel_target);
          break;
#elif defined(__i386__)
        case R_386_PC32:
          sym_addr = reinterpret_cast<ElfW(Addr)>(rel_target);
          break;
#endif
        }
      }
    }
  }

  if constexpr (IsGeneral || Mode == RelocMode::JumpTable) {
    if (r_type == R_GENERIC_JUMP_SLOT) {
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      const ElfW(Addr) result = sym_addr + get_addend_norel();
      LOGD("RELO JMP_SLOT %16p <- %16p %s", rel_target, reinterpret_cast<void *>(result), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = result;
      return true;
    }
  }

  if constexpr (IsGeneral || Mode == RelocMode::Typical) {
    // Almost all dynamic relocations are of one of these types, and most will be
    // R_GENERIC_ABSOLUTE. The platform typically uses RELR instead, but R_GENERIC_RELATIVE is
    // common in non-platform binaries.
    if (r_type == R_GENERIC_ABSOLUTE) {
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      if (found_in) {
        sym_addr = found_in->apply_memtag_if_mte_globals(sym_addr);
      }
      const ElfW(Addr) result = sym_addr + get_addend_rel();
      LOGD("RELO ABSOLUTE %16p <- %16p %s", rel_target, reinterpret_cast<void *>(result), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = result;
      return true;
    } else if (r_type == R_GENERIC_GLOB_DAT) {
      // The i386 psABI specifies that R_386_GLOB_DAT doesn't have an addend. The ARM ELF ABI
      // document (IHI0044F) specifies that R_ARM_GLOB_DAT has an addend, but Bionic isn't adding
      // it.
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      if (found_in) {
        sym_addr = found_in->apply_memtag_if_mte_globals(sym_addr);
      }
      const ElfW(Addr) result = sym_addr + get_addend_norel();
      LOGD("RELO GLOB_DAT %16p <- %16p %s", rel_target, reinterpret_cast<void *>(result), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = result;
      return true;
    } else if (r_type == R_GENERIC_RELATIVE) {
      // In practice, r_sym is always zero, but if it weren't, the linker would still look up the
      // referenced symbol (and abort if the symbol isn't found), even though it isn't used.
      count_relocation_if<IsGeneral>(kRelocRelative);
      ElfW(Addr) result = relocator.si->load_bias() + get_addend_rel();
      // MTE globals reuses the place bits for additional tag-derivation metadata for
      // R_AARCH64_RELATIVE relocations, which makes it incompatible with
      // `-Wl,--apply-dynamic-relocs`. This is enforced by lld, however there's nothing stopping
      // Android binaries (particularly prebuilts) from building with this linker flag if they're
      // not built with MTE globals. Thus, don't use the new relocation semantics if this DSO
      // doesn't have MTE globals.
      if (relocator.si->should_tag_memtag_globals()) {
        int64_t *place = static_cast<int64_t *>(rel_target);
        int64_t offset = *place;
        result = relocator.si->apply_memtag_if_mte_globals(result + offset) - offset;
      }
      LOGD("RELO RELATIVE %16p <- %16p", rel_target, reinterpret_cast<void *>(result));
      *static_cast<ElfW(Addr) *>(rel_target) = result;
      return true;
    }
  }

  if constexpr (!IsGeneral) {
    // Almost all relocations are handled above. Handle the remaining relocations below, in a
    // separate function call. The symbol lookup will be repeated, but the result should be served
    // from the 1-symbol lookup cache.
    return process_relocation_general(relocator, reloc);
  }

  switch (r_type) {
  case R_GENERIC_IRELATIVE:
    // In the linker, ifuncs are called as soon as possible so that string functions work. We must
    // not call them again. (e.g. On arm32, resolving an ifunc changes the meaning of the addend
    // from a resolver function to the implementation.)
    if (!relocator.si->is_linker()) {
      count_relocation_if<IsGeneral>(kRelocRelative);
      const ElfW(Addr) ifunc_addr = relocator.si->load_bias() + get_addend_rel();
      LOGD("RELO IRELATIVE %16p <- %16p", rel_target, reinterpret_cast<void *>(ifunc_addr));
      if (handle_text_relocs && !protect_segments()) {
        return false;
      }
      const ElfW(Addr) result = call_ifunc_resolver(ifunc_addr);
      if (handle_text_relocs && !unprotect_segments()) {
        return false;
      }
      *static_cast<ElfW(Addr) *>(rel_target) = result;
    }
    break;
  case R_GENERIC_COPY:
    // Copy relocations allow read-only data or code in a non-PIE executable to access a
    // variable from a DSO. The executable reserves extra space in its .bss section, and the
    // linker copies the variable into the extra space. The executable then exports its copy
    // to interpose the copy in the DSO.
    //
    // Bionic only supports PIE executables, so copy relocations aren't supported. The ARM and
    // AArch64 ABI documents only allow them for ET_EXEC (non-PIE) objects. See IHI0056B and
    // IHI0044F.
    LOGE("%s COPY relocations are not supported", relocator.si->realpath());
    return false;
  case R_GENERIC_TLS_TPREL:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      ElfW(Addr) tpoff = 0;
      if (found_in == nullptr) {
        // Unresolved weak relocation. Leave tpoff at 0 to resolve
        // &weak_tls_symbol to __get_tls().
      } else {
        CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
        const TlsModule &mod = get_tls_module(found_in->get_tls()->module_id);
        if (mod.static_offset != SIZE_MAX) {
          tpoff += mod.static_offset - relocator.tls_tp_base;
        } else {
          LOGE("TLS symbol \"%s\" in dlopened \"%s\" referenced from \"%s\" using IE access model", sym_name,
               found_in->realpath(), relocator.si->realpath());
          return false;
        }
      }
      tpoff += sym_addr + get_addend_rel();
      LOGD("RELO TLS_TPREL %16p <- %16p %s", rel_target, reinterpret_cast<void *>(tpoff), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = tpoff;
    }
    break;
  case R_GENERIC_TLS_DTPMOD:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      size_t module_id = 0;
      if (found_in == nullptr) {
        // Unresolved weak relocation. Evaluate the module ID to 0.
      } else {
        CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
        module_id = found_in->get_tls()->module_id;
        CHECK(module_id != kTlsUninitializedModuleId);
      }
      LOGD("RELO TLS_DTPMOD %16p <- %zu %s", rel_target, module_id, sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = module_id;
    }
    break;
  case R_GENERIC_TLS_DTPREL:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      const ElfW(Addr) result = sym_addr + get_addend_rel() - TLS_DTV_OFFSET;
      LOGD("RELO TLS_DTPREL %16p <- %16p %s", rel_target, reinterpret_cast<void *>(result), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = result;
    }
    break;

#if defined(__aarch64__) || defined(__riscv)
  // Bionic currently implements TLSDESC for arm64 and riscv64. This implementation should work
  // with other architectures, as long as the resolver functions are implemented.
  case R_GENERIC_TLSDESC:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      ElfW(Addr) addend = reloc.r_addend;
      TlsDescriptor *desc = static_cast<TlsDescriptor *>(rel_target);
      if (found_in == nullptr) {
        // Unresolved weak relocation.
        desc->func = tlsdesc_resolver_unresolved_weak;
        desc->arg = addend;
        LOGD("RELO TLSDESC %16p <- unresolved weak, addend 0x%zx %s", rel_target, static_cast<size_t>(addend),
             sym_name);
      } else {
        CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
        size_t module_id = found_in->get_tls()->module_id;
        const TlsModule &mod = get_tls_module(module_id);
        if (mod.static_offset != SIZE_MAX) {
          desc->func = tlsdesc_resolver_static;
          desc->arg = mod.static_offset - relocator.tls_tp_base + sym_addr + addend;
          LOGD("RELO TLSDESC %16p <- static (0x%zx - 0x%zx + 0x%zx + 0x%zx) %s", rel_target, mod.static_offset,
               relocator.tls_tp_base, static_cast<size_t>(sym_addr), static_cast<size_t>(addend), sym_name);
        } else {
          TlsDynamicResolverArg arg{
            .generation = mod.first_generation,
          };
          arg.index.module_id = module_id;
          arg.index.offset = sym_addr + addend;
          relocator.tlsdesc_args->push_back(arg);
          // Defer the TLSDESC relocation until the address of the TlsDynamicResolverArg object
          // is finalized.
          relocator.deferred_tlsdesc_relocs.push_back({desc, relocator.tlsdesc_args->size() - 1});
          const TlsDynamicResolverArg &desc_arg = relocator.tlsdesc_args->back();
          LOGD("RELO TLSDESC %16p <- dynamic (gen %zu, mod %zu, off %zu) %s", rel_target, desc_arg.generation,
               desc_arg.index.module_id, desc_arg.index.offset, sym_name);
        }
      }
    }
    break;
#endif // defined(__aarch64__) || defined(__riscv)

#if defined(__x86_64__)
  case R_X86_64_32:
    count_relocation_if<IsGeneral>(kRelocAbsolute);
    {
      const Elf32_Addr result = sym_addr + reloc.r_addend;
      LOGD("RELO R_X86_64_32 %16p <- 0x%08x %s", rel_target, result, sym_name);
      *static_cast<Elf32_Addr *>(rel_target) = result;
    }
    break;
  case R_X86_64_PC32:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      const ElfW(Addr) target = sym_addr + reloc.r_addend;
      const ElfW(Addr) base = reinterpret_cast<ElfW(Addr)>(rel_target);
      const Elf32_Addr result = target - base;
      LOGD("RELO R_X86_64_PC32 %16p <- 0x%08x (%16p - %16p) %s", rel_target, result, reinterpret_cast<void *>(target),
           reinterpret_cast<void *>(base), sym_name);
      *static_cast<Elf32_Addr *>(rel_target) = result;
    }
    break;
#elif defined(__i386__)
  case R_386_PC32:
    count_relocation_if<IsGeneral>(kRelocRelative);
    {
      const ElfW(Addr) target = sym_addr + get_addend_rel();
      const ElfW(Addr) base = reinterpret_cast<ElfW(Addr)>(rel_target);
      const ElfW(Addr) result = target - base;
      LOGD("RELO R_386_PC32 %16p <- 0x%08x (%16p - %16p) %s", rel_target, result, reinterpret_cast<void *>(target),
           reinterpret_cast<void *>(base), sym_name);
      *static_cast<ElfW(Addr) *>(rel_target) = result;
    }
    break;
#endif
  default:
    LOGE("unknown reloc type %d in \"%s\"", r_type, relocator.si->realpath());
    return false;
  }
  return true;
}

__attribute__((noinline)) static bool process_relocation_general(Relocator &relocator, const rel_t &reloc) {
  return process_relocation_impl<RelocMode::General>(relocator, reloc);
}

template <RelocMode Mode>
__attribute__((always_inline)) static inline bool process_relocation(Relocator &relocator, const rel_t &reloc) {
  return Mode == RelocMode::General ? process_relocation_general(relocator, reloc)
                                    : process_relocation_impl<Mode>(relocator, reloc);
}

template <RelocMode Mode>
__attribute__((noinline)) static bool plain_relocate_impl(Relocator &relocator, rel_t *rels, size_t rel_count) {
  for (size_t i = 0; i < rel_count; ++i) {
    if (!process_relocation<Mode>(relocator, rels[i])) {
      return false;
    }
  }
  return true;
}

template <RelocMode Mode>
__attribute__((noinline)) static bool packed_relocate_impl(Relocator &relocator, sleb128_decoder decoder) {
  return for_all_packed_relocs(decoder, [&](const rel_t &reloc) {
    return process_relocation<Mode>(relocator, reloc);
  });
}

static bool needs_slow_relocate_loop(const Relocator &relocator __unused) {
#if !defined(__LP64__)
  if (relocator.si->has_text_relocations())
    return true;
#endif
  // Both LD_DEBUG relocation logging and statistics need the slow path.
  // if (g_linker_debug_config.any || g_linker_debug_config.statistics) {
  //   return true;
  // }
  return false;
}

template <RelocMode OptMode, typename... Args>
static bool plain_relocate(Relocator &relocator, Args... args) {
  return needs_slow_relocate_loop(relocator) ? plain_relocate_impl<RelocMode::General>(relocator, args...)
                                             : plain_relocate_impl<OptMode>(relocator, args...);
}

template <RelocMode OptMode, typename... Args>
static bool packed_relocate(Relocator &relocator, Args... args) {
  return needs_slow_relocate_loop(relocator) ? packed_relocate_impl<RelocMode::General>(relocator, args...)
                                             : packed_relocate_impl<OptMode>(relocator, args...);
}

inline bool mte_supported() {
#if defined(__aarch64__)
  static bool supported = getauxval(AT_HWCAP2) & HWCAP2_MTE;
#else
  static bool supported = false;
#endif
  return supported;
}

inline void *get_tagged_address(const void *ptr) {
#if defined(__aarch64__)
  if (mte_supported()) {
    __asm__ __volatile__(".arch_extension mte; ldg %0, [%0]" : "+r"(ptr));
  }
#endif // aarch64
  return const_cast<void *>(ptr);
}

static void apply_relr_reloc(ElfW(Addr) offset, ElfW(Addr) load_bias, bool has_memtag_globals) {
  ElfW(Addr) destination = offset + load_bias;
  if (!has_memtag_globals) {
    *reinterpret_cast<ElfW(Addr) *>(destination) += load_bias;
    return;
  }

  ElfW(Addr) *tagged_destination =
    reinterpret_cast<ElfW(Addr) *>(get_tagged_address(reinterpret_cast<void *>(destination)));
  ElfW(Addr) tagged_value =
    reinterpret_cast<ElfW(Addr)>(get_tagged_address(reinterpret_cast<void *>(*tagged_destination + load_bias)));
  *tagged_destination = tagged_value;
}

// Process relocations in SHT_RELR section (experimental).
// Details of the encoding are described in this post:
//   https://groups.google.com/d/msg/generic-abi/bX460iggiKg/Pi9aSwwABgAJ
bool relocate_relr(const ElfW(Relr) * begin, const ElfW(Relr) * end, ElfW(Addr) load_bias, bool has_memtag_globals) {
  constexpr size_t wordsize = sizeof(ElfW(Addr));

  ElfW(Addr) base = 0;
  for (const ElfW(Relr) *current = begin; current < end; ++current) {
    ElfW(Relr) entry = *current;
    ElfW(Addr) offset;

    if ((entry & 1) == 0) {
      // Even entry: encodes the offset for next relocation.
      offset = static_cast<ElfW(Addr)>(entry);
      apply_relr_reloc(offset, load_bias, has_memtag_globals);
      // Set base offset for subsequent bitmap entries.
      base = offset + wordsize;
      continue;
    }

    // Odd entry: encodes bitmap for relocations starting at base.
    offset = base;
    while (entry != 0) {
      entry >>= 1;
      if ((entry & 1) != 0) {
        apply_relr_reloc(offset, load_bias, has_memtag_globals);
      }
      offset += wordsize;
    }

    // Advance base offset by 63 words for 64-bit platforms,
    // or 31 words for 32-bit platforms.
    base += (8 * wordsize - 1) * wordsize;
  }
  return true;
}

bool soinfo::relocate() {
  soinfo_list_t local_group;
  for (auto child : this->get_children()) {
    local_group.push_back(child);
  }
  SymbolLookupList lookup_list(fakelinker::ProxyLinker::Get().GetSoinfoGlobalGroup(this), local_group);
  lookup_list.set_dt_symbolic_lib(has_DT_SYMBOLIC() ? this : nullptr);
  VersionTracker version_tracker;
  if (!version_tracker.init(this)) {
    return false;
  }

  Relocator relocator(version_tracker, lookup_list);
  relocator.si = this;
  relocator.si_strtab = this->strtab();
  relocator.si_strtab_size = this->strtab_size();
  relocator.si_symtab = this->symtab();
  relocator.tlsdesc_args = &this->tlsdesc_args();
  // todo tls
  // relocator.tls_tp_base = static_tls_layout.
  if (relr() != nullptr) {
    const ElfW(Relr) *begin = relr();
    const ElfW(Relr) *end = relr() + relr_count();
    if (!relocate_relr(begin, end, load_bias(), should_tag_memtag_globals())) {
      return false;
    }
  }

  if (auto android_relocs = this->android_relocs()) {
    if (android_relocs_size() > 3 && android_relocs[0] == 'A' && android_relocs[1] == 'P' && android_relocs[2] == 'S' &&
        android_relocs[3] == '2') {
      const uint8_t *packed_relocs = android_relocs + 4;
      const size_t packed_relocs_size = android_relocs_size() - 4;
      if (packed_relocate<RelocMode::Typical>(relocator, sleb128_decoder(packed_relocs, packed_relocs_size))) {
        return false;
      }
    } else {
      LOGE("%s bad android relocation header.", this->get_soname());
      return false;
    }
  }

#if defined(USE_RELA)
  if (auto rela = this->rela()) {
    LOGD("[ relocating %s rela ]", realpath());
    if (!plain_relocate<RelocMode::Typical>(relocator, rela, rela_count())) {
      return false;
    }
  }
  if (auto plt_rela = this->plt_rela()) {
    LOGD("[ relocating %s plt rela ]", realpath());
    if (!plain_relocate<RelocMode::JumpTable>(relocator, plt_rela, plt_rela_count())) {
      return false;
    }
  }
#else
  if (auto rel = this->rel()) {
    LOGD("[ relocating %s rel ]", realpath());
    if (!plain_relocate<RelocMode::Typical>(relocator, rel, rel_count())) {
      return false;
    }
  }
  if (auto plt_rel = this->plt_rel()) {
    LOGD("[ relocating %s plt rel ]", realpath());
    if (!plain_relocate<RelocMode::JumpTable>(relocator, plt_rel, plt_rel_count())) {
      return false;
    }
  }
#endif
  return true;
}

template <RelocMode Mode>
__attribute__((always_inline)) static bool
process_relocation_special_impl(soinfo *si, const std::function<bool()> &unprotect_relro,
                                const symbol_relocations &relocs, const rel_t &reloc) {
  constexpr bool IsGeneral = Mode == RelocMode::General;

  void *const rel_target = reinterpret_cast<void *>(si->apply_memtag_if_mte_globals(reloc.r_offset + si->load_bias()));
  const uint32_t r_type = R_TYPE(reloc.r_info);
  const uint32_t r_sym = R_SYM(reloc.r_info);
  if (r_sym == 0) {
    return true;
  }
  const char *sym_name = si->get_string(si->symtab()[r_sym].st_name);

  if (auto itr = relocs.find(sym_name); itr != relocs.end()) {
    if (!unprotect_relro()) {
      LOGE("unprotect_relro failed");
      return false;
    }
    *static_cast<ElfW(Addr) *>(rel_target) += itr->second;
    LOGD("again relocate symbol: %s, target: %p", sym_name, (void *)*static_cast<ElfW(Addr) *>(rel_target));
  }
  return true;
}

template <RelocMode Mode>
__attribute__((noinline)) static bool
plain_relocate_special_impl(soinfo *si, const std::function<bool()> &unprotect_relro, const symbol_relocations &relocs,
                            rel_t *rels, size_t rel_count) {
  for (size_t i = 0; i < rel_count; ++i) {
    if (!process_relocation_special_impl<Mode>(si, unprotect_relro, relocs, rels[i])) {
      return false;
    }
  }
  return true;
}

template <RelocMode OptMode, typename... Args>
static bool plain_relocate_special(soinfo *si, const std::function<bool()> &unprotect_relro,
                                   const symbol_relocations &relocs, Args... args) {
  bool relocate_loop = false;
#if !defined(__LP64__)
  if (si->has_text_relocations()) {
    relocate_loop = true;
  }
#endif

  return relocate_loop ? plain_relocate_special_impl<RelocMode::General>(si, unprotect_relro, relocs, args...)
                       : plain_relocate_special_impl<OptMode>(si, unprotect_relro, relocs, args...);
}

bool soinfo::relocate_special(const symbol_relocations &relocs) {
  // While relocating a DSO with text relocations (obsolete and 32-bit only), the .text segment is
  // writable (but not executable). To call an ifunc, temporarily remap the segment as executable
  // (but not writable). Then switch it back to continue applying relocations in the segment.
#if defined(__LP64__)
  const bool handle_text_relocs = false;
  auto protect_segments = []() {
    return true;
  };
  auto unprotect_segments = []() {
    return true;
  };
#else
  soinfo *si = this;
  bool protect = false;
  const bool handle_text_relocs = si->has_text_relocations();
  auto protect_segments = [&]() {
    // Make .text executable.
    if (!protect) {
      return true;
    }
    if (fakelinker::phdr_table_protect_segments(si->phdr(), si->phnum(), si->load_bias(), si->should_pad_segments(),
                                                si->should_use_16kib_app_compat()) < 0) {
      LOGE("can't protect segments for \"%s\": %m", si->realpath());
      return false;
    }
    return true;
  };
  auto unprotect_segments = [&]() {
    if (protect) {
      return true;
    }
    protect = true;
    // Make .text writable.
    if (fakelinker::phdr_table_unprotect_segments(si->phdr(), si->phnum(), si->load_bias(), si->should_pad_segments(),
                                                  si->should_use_16kib_app_compat()) < 0) {
      LOGE("can't unprotect loadable segments for \"%s\": %m", si->realpath());
      return false;
    }
    return true;
  };
#endif
  if (handle_text_relocs) {
    unprotect_segments();
  }

  bool lazy_unprotect_relro = false;
  auto unprotect_relro = [&]() {
    if (lazy_unprotect_relro) {
      return true;
    }
    lazy_unprotect_relro =
      fakelinker::phdr_table_unprotect_gnu_relro(this->phdr(), this->phnum(), this->load_bias(),
                                                 this->should_pad_segments(), this->should_use_16kib_app_compat()) == 0;
    return lazy_unprotect_relro;
  };

#if defined(USE_RELA)
  if (auto rela = this->rela()) {
    LOGD("[ special relocating %s rela ]", realpath());
    if (!plain_relocate_special<RelocMode::Typical>(this, unprotect_relro, relocs, rela, rela_count())) {
      return false;
    }
  }
  if (auto plt_rela = this->plt_rela()) {
    LOGD("[ special relocating %s plt rela ]", realpath());
    if (!plain_relocate_special<RelocMode::JumpTable>(this, unprotect_relro, relocs, plt_rela, plt_rela_count())) {
      return false;
    }
  }
#else
  if (auto rel = this->rel()) {
    LOGD("[ special relocating %s rel ]", realpath());
    if (!plain_relocate_special<RelocMode::Typical>(this, unprotect_relro, relocs, rel, rel_count())) {
      return false;
    }
  }
  if (auto plt_rel = this->plt_rel()) {
    LOGD("[ special relocating %s plt rel ]", realpath());
    if (!plain_relocate_special<RelocMode::JumpTable>(this, unprotect_relro, relocs, plt_rel, plt_rel_count())) {
      return false;
    }
  }
#endif
  protect_segments();
  if (lazy_unprotect_relro) {
    return fakelinker::phdr_table_protect_gnu_relro(this->phdr(), this->phnum(), this->load_bias(),
                                                    this->should_pad_segments(),
                                                    this->should_use_16kib_app_compat()) == 0;
  }
  return true;
}
