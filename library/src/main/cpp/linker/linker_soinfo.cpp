//
// Created by beich on 2020/11/5.
//

#include <dlfcn.h>
#include <sys/auxv.h>

#include <maps_util.h>
#include <type.h>

#include "linker_globals.h"
#include "linker_relocs.h"
#include "linker_soinfo.h"
#include "linker_util.h"
#include "local_block_allocator.h"

#if (defined(__arm__) || defined(__aarch64__))
#include "linker_gnu_hash_neon.h"
#endif

#define LINKER_VERBOSITY_PRINT (-1)
#define LINKER_VERBOSITY_INFO  0
#define LINKER_VERBOSITY_TRACE 1
#define LINKER_VERBOSITY_DEBUG 2

#define LINKER_DEBUG_TO_LOG    1

#define TRACE_DEBUG            1
#define DO_TRACE_LOOKUP        1
#define DO_TRACE_RELO          1
#define DO_TRACE_IFUNC         1
#define TIMING                 0
#define STATS                  0

constexpr ElfW(Versym) kVersymNotNeeded = 0;
constexpr ElfW(Versym) kVersymGlobal = 1;
constexpr ElfW(Versym) kVersymHiddenBit = 0x8000;

bool useGnuHashNeon = false;

#define SOINFO_FUN(Ret, Name)       Ret (*Name)(soinfo * thiz)
#define SOINFO_CONST_FUN(Ret, Name) Ret (*Name)(const soinfo *thiz)

struct SoinfoFunTable {
  SOINFO_FUN(const char *, name_);

  SOINFO_FUN(const ElfW(Phdr) *, phdr);

  SOINFO_FUN(size_t, phnum);

  SOINFO_FUN(ElfW(Addr), base);

  SOINFO_FUN(size_t, size);

  SOINFO_FUN(ElfW(Dyn) *, dynamic);

  SOINFO_FUN(soinfo *, next);

  SOINFO_FUN(uint32_t &, flags_);

  SOINFO_FUN(const char *, strtab_);

  SOINFO_FUN(ElfW(Sym) *, symtab_);

  SOINFO_FUN(size_t, nbucket_);

  SOINFO_FUN(size_t, nchain_);

  ANDROID_LE_L1 SOINFO_FUN(unsigned *, bucket_);

  ANDROID_LE_L1 SOINFO_FUN(unsigned *, chain_);

  ANDROID_GE_M SOINFO_FUN(uint32_t *, bucket_M);

  ANDROID_GE_M SOINFO_FUN(uint32_t *, chain_M);

#ifndef __LP64__

  SOINFO_FUN(ElfW(Addr) **, plt_got_);

#endif

#ifdef USE_RELA
  SOINFO_FUN(ElfW(Rela) *&, plt_rela_);

  SOINFO_FUN(size_t &, plt_rela_count_);

  SOINFO_FUN(ElfW(Rela) *&, rela_);

  SOINFO_FUN(size_t &, rela_count_);
#else

  SOINFO_FUN(ElfW(Rel) *&, plt_rel_);

  SOINFO_FUN(size_t &, plt_rel_count_);

  SOINFO_FUN(ElfW(Rel) *&, rel_);

  SOINFO_FUN(size_t &, rel_count_);

#endif

  ANDROID_LE_O1 SOINFO_FUN(linker_function_t *, preinit_array_);

  ANDROID_GE_P SOINFO_FUN(linker_ctor_function_t *, preinit_array_P);

  SOINFO_FUN(size_t, preinit_array_count_);

  ANDROID_LE_O1 SOINFO_FUN(linker_function_t *, init_array_);

  ANDROID_GE_P SOINFO_FUN(linker_ctor_function_t *, init_array_P);

  SOINFO_FUN(size_t, init_array_count_);

  ANDROID_LE_O1 SOINFO_FUN(linker_function_t *, fini_array_);

  ANDROID_GE_P SOINFO_FUN(linker_dtor_function_t *, fini_array_P);

  SOINFO_FUN(size_t, fini_array_count_);

  ANDROID_LE_O1 SOINFO_FUN(linker_function_t, init_func_);

  ANDROID_GE_P SOINFO_FUN(linker_ctor_function_t, init_func_P);

  ANDROID_LE_O1 SOINFO_FUN(linker_function_t, fini_func_);

  ANDROID_GE_P SOINFO_FUN(linker_dtor_function_t, fini_func_P);

#ifdef __arm__
  SOINFO_FUN(uint32_t *, ARM_exidx);
  SOINFO_FUN(size_t, ARM_exidx_count);
#endif

  SOINFO_FUN(size_t, ref_count_);

  SOINFO_FUN(link_map &, link_map_head);

  SOINFO_FUN(bool, constructors_called);

  SOINFO_FUN(ElfW(Addr), load_bias);

#ifndef __LP64__

  SOINFO_FUN(bool, has_text_relocations);

#endif

  SOINFO_FUN(bool, has_DT_SYMBOLIC);

  SOINFO_FUN(uint32_t, version_);

  SOINFO_FUN(dev_t, st_dev_);

  SOINFO_FUN(ino_t, st_ino_);

  ANDROID_LE_S SOINFO_FUN(soinfo_list_t &, children_);
  // 实际类型被内联
  ANDROID_GE_T SOINFO_FUN(soinfo_list_t_T &, children_T);

  ANDROID_LE_S SOINFO_FUN(soinfo_list_t &, parents_);

  ANDROID_GE_T SOINFO_FUN(soinfo_list_t_T &, parents_T);

  ANDROID_GE_L1 SOINFO_FUN(off64_t, file_offset_);

  ANDROID_GE_M SOINFO_FUN(uint32_t &, rtld_flags_);

  ANDROID_LE_L1 SOINFO_FUN(int &, rtld_flags_L);

  ANDROID_GE_M SOINFO_FUN(uint32_t &, dt_flags_1_);

  ANDROID_GE_L1 SOINFO_FUN(size_t, strtab_size_);

  ANDROID_GE_M SOINFO_FUN(size_t, gnu_nbucket_);

  ANDROID_GE_M SOINFO_FUN(uint32_t *, gnu_bucket_);

  ANDROID_GE_M SOINFO_FUN(uint32_t *, gnu_chain_);

  ANDROID_GE_M SOINFO_FUN(uint32_t, gnu_maskwords_);

  ANDROID_GE_M SOINFO_FUN(uint32_t, gnu_shift2_);

  ANDROID_GE_M SOINFO_FUN(ElfW(Addr) *, gnu_bloom_filter_);

  ANDROID_GE_M SOINFO_FUN(soinfo *, local_group_root_);

  ANDROID_GE_M SOINFO_FUN(uint8_t *, android_relocs_);

  ANDROID_GE_M SOINFO_FUN(size_t, android_relocs_size_);

  ANDROID_GE_M SOINFO_FUN(const char *, soname_);

  ANDROID_GE_M SOINFO_FUN(const char *, realpath_);

  ANDROID_GE_M SOINFO_FUN(const ElfW(Versym) *, versym_);

  ANDROID_GE_M SOINFO_FUN(ElfW(Addr), verdef_ptr_);

  ANDROID_GE_M SOINFO_FUN(size_t, verdef_cnt_);

  ANDROID_GE_M SOINFO_FUN(ElfW(Addr), verneed_ptr_);

  ANDROID_GE_M SOINFO_FUN(size_t, verneed_cnt_);

  ANDROID_GE_M SOINFO_FUN(int, target_sdk_version_);

  ANDROID_GE_N SOINFO_FUN(std::vector<std::string> &, dt_runpath_);

  ANDROID_GE_N SOINFO_FUN(android_namespace_t *&, primary_namespace_);

  ANDROID_GE_N SOINFO_FUN(android_namespace_list_t &, secondary_namespaces_);

  ANDROID_GE_T SOINFO_FUN(android_namespace_list_t_T &, secondary_namespaces_T);

  ANDROID_GE_N SOINFO_FUN(uintptr_t, handle_);

  ANDROID_GE_P SOINFO_FUN(ElfW(Relr) *, relr_);

  ANDROID_GE_P SOINFO_FUN(size_t, relr_count_);

  ANDROID_GE_Q SOINFO_FUN(std::unique_ptr<soinfo_tls> &, tls_);

  ANDROID_GE_Q SOINFO_FUN(std::vector<TlsDynamicResolverArg> &, tlsdesc_args_);

  ANDROID_GE_T SOINFO_FUN(ElfW(Addr), gap_start_);

  ANDROID_GE_T SOINFO_FUN(size_t, gap_size_);

  /**
   *  All version common
   */
  SOINFO_FUN(const char *, get_soname);
};

static SoinfoFunTable funTable;

#if defined(__aarch64__)
/**
 * Provides information about hardware capabilities to ifunc resolvers.
 *
 * Starting with API level 30, ifunc resolvers on arm64 are passed two
 * arguments. The first is a uint64_t whose value is equal to
 * getauxval(AT_HWCAP) | _IFUNC_ARG_HWCAP. The second is a pointer to a data
 * structure of this type. Prior to API level 30, no arguments are passed to
 * ifunc resolvers. Code that wishes to be compatible with prior API levels
 * should not accept any arguments in the resolver.
 */
typedef struct __ifunc_arg_t {
  /** Set to sizeof(__ifunc_arg_t). */
  unsigned long _size;

  /** Set to getauxval(AT_HWCAP). */
  unsigned long _hwcap;

  /** Set to getauxval(AT_HWCAP2). */
  unsigned long _hwcap2;
} __ifunc_arg_t;

/**
 * If this bit is set in the first argument to an ifunc resolver, indicates that
 * the second argument is a pointer to a data structure of type __ifunc_arg_t.
 * This bit is always set on Android starting with API level 30.
 */
#define _IFUNC_ARG_HWCAP (1ULL << 62)
#endif

#define SOINFO_MEMBER_NULL(Type, Name, ...) funTable.Name = nullptr;

#define SOINFO_MEMBER_WRAP(Type, Name)                                                                                 \
  funTable.Name = [](soinfo *thiz) -> member_type_trait<decltype(&Type::Name)>::type {                                 \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

#define SOINFO_MEMBER_VER_WRAP(Type, Name, Ver)                                                                        \
  funTable.Name##Ver = [](soinfo *thiz) -> member_type_trait<decltype(&Type::Name)>::type {                            \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

#define SOINFO_MEMBER_CAST_WRAP(Type, Name, Ret)                                                                       \
  funTable.Name = [](soinfo *thiz) -> Ret {                                                                            \
    return (Ret) reinterpret_cast<Type *>(thiz)->Name;                                                                 \
  }

#define SOINFO_MEMBER_STRING_TO_CHARP_WRAP(Type, Name)                                                                 \
  funTable.Name = [](soinfo *thiz) -> const char * {                                                                   \
    return reinterpret_cast<Type *>(thiz)->Name.c_str();                                                               \
  }

#define SOINFO_MEMBER_REF_WRAP(Type, Name)                                                                             \
  funTable.Name = [](soinfo *thiz) -> member_ref_type_trait<decltype(&Type::Name)>::type {                             \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

#define SOINFO_MEMBER_REF_VER_WRAP(Type, Name, Ver)                                                                    \
  funTable.Name##Ver = [](soinfo *thiz) -> member_ref_type_trait<decltype(&Type::Name)>::type {                        \
    return reinterpret_cast<Type *>(thiz)->Name;                                                                       \
  }

ElfW(Addr) __bionic_call_ifunc_resolver(ElfW(Addr) resolver_addr) {
#if defined(__aarch64__)
  typedef ElfW(Addr) (*ifunc_resolver_t)(uint64_t, __ifunc_arg_t *);
  static __ifunc_arg_t arg;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    arg._size = sizeof(__ifunc_arg_t);
    arg._hwcap = getauxval(AT_HWCAP);
    arg._hwcap2 = getauxval(AT_HWCAP2);
  }
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)(arg._hwcap | _IFUNC_ARG_HWCAP, &arg);
#elif defined(__arm__)
  typedef ElfW(Addr) (*ifunc_resolver_t)(unsigned long);
  static unsigned long hwcap;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    hwcap = getauxval(AT_HWCAP);
  }
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)(hwcap);
#else
  typedef ElfW(Addr) (*ifunc_resolver_t)();
  return reinterpret_cast<ifunc_resolver_t>(resolver_addr)();
#endif
}

inline bool check_symbol_version(const ElfW(Versym) * ver_table, uint32_t sym_idx, const ElfW(Versym) verneed) {
  if (ver_table == nullptr)
    return true;
  const uint32_t verdef = ver_table[sym_idx];
  return (verneed == kVersymNotNeeded) ? !(verdef & kVersymHiddenBit) : verneed == (verdef & ~kVersymHiddenBit);
}

inline bool is_symbol_global_and_defined(soinfo *si, const ElfW(Sym) * s) {
  if (ELF_ST_BIND(s->st_info) == STB_GLOBAL || ELF_ST_BIND(s->st_info) == STB_WEAK) {
    return s->st_shndx != SHN_UNDEF;
  } else if (ELF_ST_BIND(s->st_info) != STB_LOCAL) {
    LOGW("Warning: unexpected ST_BIND value: %d for \"%s\" in \"%s\" (ignoring)", ELF_ST_BIND(s->st_info),
         si->get_string(s->st_name), si->realpath());
  }
  return false;
}

template <typename F>
static bool for_each_verdef(soinfo *si, F functor) {
  if (!si->has_min_version(2)) {
    return true;
  }

  uintptr_t verdef_ptr = si->get_verdef_ptr();
  if (verdef_ptr == 0) {
    return true;
  }

  size_t offset = 0;

  size_t verdef_cnt = si->get_verdef_cnt();
  for (size_t i = 0; i < verdef_cnt; ++i) {
    const ElfW(Verdef) *verdef = reinterpret_cast<ElfW(Verdef) *>(verdef_ptr + offset);
    size_t verdaux_offset = offset + verdef->vd_aux;
    offset += verdef->vd_next;

    if (verdef->vd_version != 1) {
      LOGE("unsupported verdef[%zd] vd_version: %d (expected 1) library: %s", i, verdef->vd_version, si->realpath());
      return false;
    }

    if ((verdef->vd_flags & VER_FLG_BASE) != 0) {
      continue;
    }

    if (verdef->vd_cnt == 0) {
      LOGE("invalid verdef[%zd] vd_cnt == 0 (version without a name)", i);
      return false;
    }
    const ElfW(Verdaux) *verdaux = reinterpret_cast<ElfW(Verdaux) *>(verdef_ptr + verdaux_offset);

    if (functor(i, verdef, verdaux) == true) {
      break;
    }
  }

  return true;
}

ElfW(Versym) find_verdef_version_index(soinfo *si, const version_info *vi) {
  if (vi == nullptr) {
    return kVersymNotNeeded;
  }

  ElfW(Versym) result = kVersymGlobal;

  if (!for_each_verdef(si, [&](size_t, const ElfW(Verdef) * verdef, const ElfW(Verdaux) * verdaux) {
        if (verdef->vd_hash == vi->elf_hash && strcmp(vi->name, si->get_string(verdaux->vda_name)) == 0) {
          result = verdef->vd_ndx;
          return true;
        }
        return false;
      })) {
    CHECK_OUTPUT(false, "invalid verdef after prelinking: %s", si->realpath());
  }
  return result;
}

static bool is_lookup_tracing_enabled() {
  if (fakelinker::ProxyLinker::Get().GetGLdDebugVerbosity() == nullptr) {
    return false;
  }
  return *fakelinker::ProxyLinker::Get().GetGLdDebugVerbosity() > LINKER_VERBOSITY_TRACE && DO_TRACE_LOOKUP;
}

ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr) {
  ElfW(Addr) ifunc_addr = __bionic_call_ifunc_resolver(resolver_addr);
  return ifunc_addr;
}

static int find_import_symbol_index_by_name(soinfo *si, const char *name, bool &type) {
#ifdef USE_RELA
  ElfW(Rela) *start = si->plt_rela() == nullptr ? si->rela() : si->plt_rela();
  ElfW(Rela) *end = si->plt_rela() == nullptr ? si->rela() + si->rela_count() : si->plt_rela() + si->plt_rela_count();
  type = si->plt_rela() != nullptr;
#else
  ElfW(Rel) *start = si->plt_rel() == nullptr ? si->rel() : si->plt_rel();
  ElfW(Rel) *end = si->plt_rel() == nullptr ? si->rel() + si->rel_count() : si->plt_rel() + si->plt_rel_count();
  type = si->plt_rel() != nullptr;
#endif
  int index = 0;
  const char *sym_name;
  for (; start < end; start++, index++) {
    sym_name = si->get_string(si->symtab()[R_SYM(start->r_info)].st_name);
    if (strcmp(sym_name, name) == 0) {
      return index;
    }
  }
  return -1;
}

SymbolLookupList::SymbolLookupList(soinfo *si) :
    sole_lib_(si->get_lookup_lib()), begin_(&sole_lib_), end_(&sole_lib_ + 1) {
  CHECK(si != nullptr);
  slow_path_count_ += is_lookup_tracing_enabled();
  slow_path_count_ += sole_lib_.needs_sysv_lookup();
}

SymbolLookupList::SymbolLookupList(const soinfo_list_t &global_group, const soinfo_list_t &local_group) {
  slow_path_count_ += is_lookup_tracing_enabled();
  libs_.reserve(1 + global_group.size() + local_group.size());

  // Reserve a space in front for DT_SYMBOLIC lookup.
  libs_.push_back(SymbolLookupLib{});

  global_group.for_each([this](soinfo *si) {
    libs_.push_back(si->get_lookup_lib());
    slow_path_count_ += libs_.back().needs_sysv_lookup();
  });

  local_group.for_each([this](soinfo *si) {
    libs_.push_back(si->get_lookup_lib());
    slow_path_count_ += libs_.back().needs_sysv_lookup();
  });

  begin_ = &libs_[1];
  end_ = &libs_[0] + libs_.size();
}

void SymbolLookupList::set_dt_symbolic_lib(soinfo *lib) {
  CHECK(!libs_.empty());
  slow_path_count_ -= libs_[0].needs_sysv_lookup();
  libs_[0] = lib ? lib->get_lookup_lib() : SymbolLookupLib();
  slow_path_count_ += libs_[0].needs_sysv_lookup();
  begin_ = lib ? &libs_[0] : &libs_[1];
}

uint32_t SymbolName::elf_hash() {
  if (!has_elf_hash_) {
    elf_hash_ = calculate_elf_hash(name_);
    has_elf_hash_ = true;
  }

  return elf_hash_;
}

uint32_t SymbolName::gnu_hash() {
#if (defined(__arm__) || defined(__aarch64__))
  if (!has_gnu_hash_ && useGnuHashNeon) {
    gnu_hash_ = calculate_gnu_hash_neon(name_).first;
    has_gnu_hash_ = true;
    return gnu_hash_;
  }
#endif
  gnu_hash_ = calculate_gnu_hash(name_);
  has_gnu_hash_ = true;
  return gnu_hash_;
}

enum class RelocMode {
  // Fast path for JUMP_SLOT relocations.
  JumpTable,
  // Fast path for typical relocations: ABSOLUTE, GLOB_DAT, or RELATIVE.
  Typical,
  // Handle all relocation types, relocations in text sections, and
  // statistics/tracing.
  General,
};

template <RelocMode Mode>
static bool process_relocation(soinfo *so, const rel_t &reloc, symbol_relocations &rels) {
  // rel 通用
  void *const rel_target = reinterpret_cast<void *>(reloc.r_offset + so->load_bias());
  const uint32_t r_type = R_TYPE(reloc.r_info);
  const uint32_t r_sym = R_SYM(reloc.r_info);
  const char *sym_name = r_sym != 0 ? so->get_string(so->symtab()[r_sym].st_name) : nullptr;
  ElfW(Addr) sym_addr = 0;

  if (__predict_false(r_type == R_GENERIC_NONE)) { // R_GENERIC_NONE
    return false;
  }
  if (is_tls_reloc(r_type)) {
    return false;
  }
  if (__predict_false(sym_name == nullptr)) {
    return false;
  }
  if (ELF_ST_BIND(so->symtab()[r_sym].st_info) == STB_LOCAL) {
    return false;
  }
#if defined(USE_RELA)
  auto get_addend_rel = [&]() -> ElfW(Addr) {
    return reloc.r_addend;
  };
  auto get_addend_norel = [&]() -> ElfW(Addr) {
    return reloc.r_addend;
  };
#else
  // 这种情况下已经重定位了就无法再获得该
  // addend,但是通常我们要重定位的符号用不上
  auto get_addend_rel = [&]() -> ElfW(Addr) {
    LOGE("Error: Symbols that may be wrong are being relocated");
    return *static_cast<ElfW(Addr) *>(rel_target);
  };
  auto get_addend_norel = [&]() -> ElfW(Addr) {
    return 0;
  };
#endif
  ElfW(Addr) orig = *static_cast<ElfW(Addr) *>(rel_target);
  for (size_t i = 0, e = rels.size(); i < e; ++i) {
    if (strcmp(sym_name, rels[i].name) == 0) {
      sym_addr = rels[i].sym_address;
      if (Mode == RelocMode::JumpTable) {
        if (r_type == R_GENERIC_JUMP_SLOT) {
          *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_norel();
          LOGV("Relocation symbol JumpTable: %s, original address: %p, new "
               "address: %p",
               sym_name, reinterpret_cast<void *>(orig),
               reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
          return true;
        }
      }
      if (Mode == RelocMode::Typical) {
        if (r_type == R_GENERIC_ABSOLUTE) {
          *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_rel();
          LOGV("Relocation symbol Typical ABSOLUTE: %s, original address: %16p, "
               " new address: %16p",
               sym_name, reinterpret_cast<void *>(orig),
               reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
          return true;
        } else if (r_type == R_GENERIC_GLOB_DAT) {
          *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_norel();
          LOGV("Relocation symbol Typical GLOB_DAT: %s, original address: %16p, "
               " new address: %16p",
               sym_name, reinterpret_cast<void *>(orig),
               reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
          return true;
        }
      }
      return false;
    }
  }
  return true;
}

template <RelocMode OptMode>
static bool plain_relocate_impl(soinfo *so, rel_t *rels, size_t rel_count, symbol_relocations &symbols) {
  for (size_t i = 0; i < rel_count; ++i) {
    process_relocation<OptMode>(so, rels[i], symbols);
  }
  return true;
}

// bool VersionTracker::init(const soinfo *si_from) {
//	if (!si_from->has_min_version(2)) {
//		return true;
//	}
//
//	return init_verneed(si_from) && init_verdef(si_from);
// }
//
// const version_info *VersionTracker::get_version_info(ElfW(Versym)
// source_symver) const { 	if (source_symver < 2 || 		source_symver
// >=
// version_infos.size() || 		version_infos[source_symver].name ==
// nullptr) { 		return nullptr;
//	}
//
//	return &version_infos[source_symver];
// }

static void InitApiT() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoT, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoT, phdr);
  SOINFO_MEMBER_WRAP(soinfoT, phnum);
  SOINFO_MEMBER_WRAP(soinfoT, base);
  SOINFO_MEMBER_WRAP(soinfoT, size);
  SOINFO_MEMBER_WRAP(soinfoT, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoT, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoT, flags_);
  SOINFO_MEMBER_WRAP(soinfoT, strtab_);
  SOINFO_MEMBER_WRAP(soinfoT, symtab_);
  SOINFO_MEMBER_WRAP(soinfoT, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoT, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoT, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoT, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoT, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoT, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoT, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, rel_count_);
#endif
  SOINFO_MEMBER_VER_WRAP(soinfoT, preinit_array_, P);
  SOINFO_MEMBER_WRAP(soinfoT, preinit_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoT, init_array_, P);
  SOINFO_MEMBER_WRAP(soinfoT, init_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoT, fini_array_, P);
  SOINFO_MEMBER_WRAP(soinfoT, fini_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoT, init_func_, P);
  SOINFO_MEMBER_VER_WRAP(soinfoT, fini_func_, P);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoT, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoT, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoT, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoT, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoT, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoT, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoT, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoT, version_);
  SOINFO_MEMBER_WRAP(soinfoT, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoT, st_ino_);
  SOINFO_MEMBER_REF_VER_WRAP(soinfoT, children_, T);
  SOINFO_MEMBER_REF_VER_WRAP(soinfoT, parents_, T);
  SOINFO_MEMBER_WRAP(soinfoT, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoT, strtab_size_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoT, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoT, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoT, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoT, android_relocs_size_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoT, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoT, realpath_);
  SOINFO_MEMBER_WRAP(soinfoT, versym_);
  SOINFO_MEMBER_WRAP(soinfoT, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoT, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoT, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoT, verneed_cnt_);
  SOINFO_MEMBER_WRAP(soinfoT, target_sdk_version_);

  SOINFO_MEMBER_REF_WRAP(soinfoT, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, primary_namespace_);
  SOINFO_MEMBER_REF_VER_WRAP(soinfoT, secondary_namespaces_, T);
  SOINFO_MEMBER_WRAP(soinfoT, handle_);
  SOINFO_MEMBER_WRAP(soinfoT, relr_);
  SOINFO_MEMBER_WRAP(soinfoT, relr_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, tls_);
  SOINFO_MEMBER_REF_WRAP(soinfoT, tlsdesc_args_);
  SOINFO_MEMBER_WRAP(soinfoT, gap_start_);
  SOINFO_MEMBER_WRAP(soinfoT, gap_size_);
}

static void InitApiS() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoS, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoS, phdr);
  SOINFO_MEMBER_WRAP(soinfoS, phnum);
  SOINFO_MEMBER_WRAP(soinfoS, base);
  SOINFO_MEMBER_WRAP(soinfoS, size);
  SOINFO_MEMBER_WRAP(soinfoS, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoS, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoS, flags_);
  SOINFO_MEMBER_WRAP(soinfoS, strtab_);
  SOINFO_MEMBER_WRAP(soinfoS, symtab_);
  SOINFO_MEMBER_WRAP(soinfoS, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoS, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoS, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoS, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoS, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoS, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoS, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, rel_count_);
#endif
  SOINFO_MEMBER_VER_WRAP(soinfoS, preinit_array_, P);
  SOINFO_MEMBER_WRAP(soinfoS, preinit_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoS, init_array_, P);
  SOINFO_MEMBER_WRAP(soinfoS, init_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoS, fini_array_, P);
  SOINFO_MEMBER_WRAP(soinfoS, fini_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoS, init_func_, P);
  SOINFO_MEMBER_VER_WRAP(soinfoS, fini_func_, P);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoS, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoS, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoS, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoS, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoS, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoS, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoS, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoS, version_);

  SOINFO_MEMBER_WRAP(soinfoS, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoS, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, parents_);

  SOINFO_MEMBER_WRAP(soinfoS, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoS, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoS, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoS, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoS, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoS, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoS, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoS, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoS, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoS, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoS, android_relocs_size_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoS, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoS, realpath_);
  SOINFO_MEMBER_WRAP(soinfoS, versym_);
  SOINFO_MEMBER_WRAP(soinfoS, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoS, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoS, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoS, verneed_cnt_);
  SOINFO_MEMBER_WRAP(soinfoS, target_sdk_version_);

  SOINFO_MEMBER_REF_WRAP(soinfoS, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoS, handle_);

  SOINFO_MEMBER_WRAP(soinfoS, relr_);
  SOINFO_MEMBER_WRAP(soinfoS, relr_count_);

  SOINFO_MEMBER_REF_WRAP(soinfoS, tls_);
  SOINFO_MEMBER_REF_WRAP(soinfoS, tlsdesc_args_);

  SOINFO_MEMBER_WRAP(soinfoS, gap_start_);
  SOINFO_MEMBER_WRAP(soinfoS, gap_size_);
}

static void InitApiQ() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoQ, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoQ, phdr);
  SOINFO_MEMBER_WRAP(soinfoQ, phnum);
  SOINFO_MEMBER_WRAP(soinfoQ, base);
  SOINFO_MEMBER_WRAP(soinfoQ, size);
  SOINFO_MEMBER_WRAP(soinfoQ, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoQ, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, flags_);
  SOINFO_MEMBER_WRAP(soinfoQ, strtab_);
  SOINFO_MEMBER_WRAP(soinfoQ, symtab_);
  SOINFO_MEMBER_WRAP(soinfoQ, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoQ, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoQ, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoQ, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoQ, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, rel_count_);
#endif
  SOINFO_MEMBER_VER_WRAP(soinfoQ, preinit_array_, P);
  SOINFO_MEMBER_WRAP(soinfoQ, preinit_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, init_array_, P);
  SOINFO_MEMBER_WRAP(soinfoQ, init_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, fini_array_, P);
  SOINFO_MEMBER_WRAP(soinfoQ, fini_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, init_func_, P);
  SOINFO_MEMBER_VER_WRAP(soinfoQ, fini_func_, P);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoQ, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoQ, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoQ, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoQ, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoQ, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoQ, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoQ, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoQ, version_);

  SOINFO_MEMBER_WRAP(soinfoQ, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoQ, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, parents_);

  SOINFO_MEMBER_WRAP(soinfoQ, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoQ, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoQ, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoQ, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoQ, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoQ, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoQ, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoQ, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoQ, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoQ, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoQ, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoQ, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoQ, realpath_);
  SOINFO_MEMBER_WRAP(soinfoQ, versym_);
  SOINFO_MEMBER_WRAP(soinfoQ, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoQ, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoQ, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoQ, verneed_cnt_);
  SOINFO_MEMBER_WRAP(soinfoQ, target_sdk_version_);

  SOINFO_MEMBER_REF_WRAP(soinfoQ, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoQ, handle_);

  SOINFO_MEMBER_WRAP(soinfoQ, relr_);
  SOINFO_MEMBER_WRAP(soinfoQ, relr_count_);

  SOINFO_MEMBER_REF_WRAP(soinfoQ, tls_);
  SOINFO_MEMBER_REF_WRAP(soinfoQ, tlsdesc_args_);
}

static void InitApiP() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoP, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoP, phdr);
  SOINFO_MEMBER_WRAP(soinfoP, phnum);
  SOINFO_MEMBER_WRAP(soinfoP, base);
  SOINFO_MEMBER_WRAP(soinfoP, size);
  SOINFO_MEMBER_WRAP(soinfoP, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoP, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoP, flags_);
  SOINFO_MEMBER_WRAP(soinfoP, strtab_);
  SOINFO_MEMBER_WRAP(soinfoP, symtab_);
  SOINFO_MEMBER_WRAP(soinfoP, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoP, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoP, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoP, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoP, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoP, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoP, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, rel_count_);
#endif
  SOINFO_MEMBER_VER_WRAP(soinfoP, preinit_array_, P);
  SOINFO_MEMBER_WRAP(soinfoP, preinit_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoP, init_array_, P);
  SOINFO_MEMBER_WRAP(soinfoP, init_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoP, fini_array_, P);
  SOINFO_MEMBER_WRAP(soinfoP, fini_array_count_);
  SOINFO_MEMBER_VER_WRAP(soinfoP, init_func_, P);
  SOINFO_MEMBER_VER_WRAP(soinfoP, fini_func_, P);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoP, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoP, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoP, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoP, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoP, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoP, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoP, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoP, version_);

  SOINFO_MEMBER_WRAP(soinfoP, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoP, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, parents_);

  SOINFO_MEMBER_WRAP(soinfoP, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoP, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoP, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoP, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoP, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoP, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoP, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoP, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoP, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoP, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoP, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoP, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoP, realpath_);
  SOINFO_MEMBER_WRAP(soinfoP, versym_);
  SOINFO_MEMBER_WRAP(soinfoP, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoP, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoP, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoP, verneed_cnt_);
  SOINFO_MEMBER_CAST_WRAP(soinfoP, target_sdk_version_, int);
  SOINFO_MEMBER_REF_WRAP(soinfoP, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoP, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoP, handle_);

  SOINFO_MEMBER_WRAP(soinfoP, relr_);
  SOINFO_MEMBER_WRAP(soinfoP, relr_count_);
}

static void InitApiO() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoO, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoO, phdr);
  SOINFO_MEMBER_WRAP(soinfoO, phnum);
  SOINFO_MEMBER_WRAP(soinfoO, base);
  SOINFO_MEMBER_WRAP(soinfoO, size);
  SOINFO_MEMBER_WRAP(soinfoO, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoO, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoO, flags_);
  SOINFO_MEMBER_WRAP(soinfoO, strtab_);
  SOINFO_MEMBER_WRAP(soinfoO, symtab_);
  SOINFO_MEMBER_WRAP(soinfoO, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoO, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoO, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoO, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoO, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoO, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoO, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoO, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoO, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoO, init_array_);
  SOINFO_MEMBER_WRAP(soinfoO, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoO, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoO, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoO, init_func_);
  SOINFO_MEMBER_WRAP(soinfoO, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoO, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoO, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoO, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoO, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoO, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoO, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoO, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoO, version_);

  SOINFO_MEMBER_WRAP(soinfoO, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoO, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, parents_);

  SOINFO_MEMBER_WRAP(soinfoO, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoO, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoO, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoO, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoO, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoO, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoO, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoO, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoO, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoO, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoO, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoO, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoO, realpath_);
  SOINFO_MEMBER_WRAP(soinfoO, versym_);
  SOINFO_MEMBER_WRAP(soinfoO, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoO, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoO, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoO, verneed_cnt_);
  SOINFO_MEMBER_CAST_WRAP(soinfoO, target_sdk_version_, int);

  SOINFO_MEMBER_REF_WRAP(soinfoO, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoO, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoO, handle_);
}

static void InitApiN1() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoN1, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoN1, phdr);
  SOINFO_MEMBER_WRAP(soinfoN1, phnum);
  SOINFO_MEMBER_WRAP(soinfoN1, base);
  SOINFO_MEMBER_WRAP(soinfoN1, size);
  SOINFO_MEMBER_WRAP(soinfoN1, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoN1, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, flags_);
  SOINFO_MEMBER_WRAP(soinfoN1, strtab_);
  SOINFO_MEMBER_WRAP(soinfoN1, symtab_);
  SOINFO_MEMBER_WRAP(soinfoN1, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoN1, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoN1, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoN1, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoN1, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoN1, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoN1, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoN1, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoN1, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN1, init_array_);
  SOINFO_MEMBER_WRAP(soinfoN1, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN1, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoN1, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN1, init_func_);
  SOINFO_MEMBER_WRAP(soinfoN1, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoN1, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoN1, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoN1, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoN1, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoN1, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoN1, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoN1, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoN1, version_);

  SOINFO_MEMBER_WRAP(soinfoN1, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoN1, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, parents_);

  SOINFO_MEMBER_WRAP(soinfoN1, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoN1, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoN1, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoN1, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoN1, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoN1, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoN1, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoN1, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoN1, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoN1, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoN1, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoN1, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoN1, realpath_);
  SOINFO_MEMBER_WRAP(soinfoN1, versym_);
  SOINFO_MEMBER_WRAP(soinfoN1, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoN1, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoN1, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoN1, verneed_cnt_);
  SOINFO_MEMBER_CAST_WRAP(soinfoN1, target_sdk_version_, int);

  SOINFO_MEMBER_REF_WRAP(soinfoN1, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoN1, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoN1, handle_);
}

static void InitApiN() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoN, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoN, phdr);
  SOINFO_MEMBER_WRAP(soinfoN, phnum);
  SOINFO_MEMBER_WRAP(soinfoN, base);
  SOINFO_MEMBER_WRAP(soinfoN, size);
  SOINFO_MEMBER_WRAP(soinfoN, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoN, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoN, flags_);
  SOINFO_MEMBER_WRAP(soinfoN, strtab_);
  SOINFO_MEMBER_WRAP(soinfoN, symtab_);
  SOINFO_MEMBER_WRAP(soinfoN, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoN, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoN, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoN, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoN, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoN, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoN, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoN, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoN, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN, init_array_);
  SOINFO_MEMBER_WRAP(soinfoN, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoN, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoN, init_func_);
  SOINFO_MEMBER_WRAP(soinfoN, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoN, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoN, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoN, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoN, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoN, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoN, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoN, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoN, version_);
  SOINFO_MEMBER_WRAP(soinfoN, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoN, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, parents_);

  SOINFO_MEMBER_WRAP(soinfoN, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoN, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoN, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoN, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoN, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoN, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoN, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoN, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoN, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoN, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoN, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoN, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoN, realpath_);
  SOINFO_MEMBER_WRAP(soinfoN, versym_);
  SOINFO_MEMBER_WRAP(soinfoN, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoN, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoN, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoN, verneed_cnt_);
  SOINFO_MEMBER_CAST_WRAP(soinfoN, target_sdk_version_, int);

  SOINFO_MEMBER_REF_WRAP(soinfoN, dt_runpath_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, primary_namespace_);
  SOINFO_MEMBER_REF_WRAP(soinfoN, secondary_namespaces_);
  SOINFO_MEMBER_WRAP(soinfoN, handle_);
}

static void InitApiM() {
#ifdef __work_around_b_24465209__
  SOINFO_MEMBER_WRAP(soinfoM, name_);
#endif
  SOINFO_MEMBER_WRAP(soinfoM, phdr);
  SOINFO_MEMBER_WRAP(soinfoM, phnum);
  SOINFO_MEMBER_WRAP(soinfoM, base);
  SOINFO_MEMBER_WRAP(soinfoM, size);
  SOINFO_MEMBER_WRAP(soinfoM, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoM, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoM, flags_);
  SOINFO_MEMBER_WRAP(soinfoM, strtab_);
  SOINFO_MEMBER_WRAP(soinfoM, symtab_);
  SOINFO_MEMBER_WRAP(soinfoM, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoM, nchain_);
  SOINFO_MEMBER_VER_WRAP(soinfoM, bucket_, M);
  SOINFO_MEMBER_VER_WRAP(soinfoM, chain_, M);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoM, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoM, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoM, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoM, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoM, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoM, init_array_);
  SOINFO_MEMBER_WRAP(soinfoM, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoM, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoM, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoM, init_func_);
  SOINFO_MEMBER_WRAP(soinfoM, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoM, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoM, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoM, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoM, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoM, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoM, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoM, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoM, version_);

  SOINFO_MEMBER_WRAP(soinfoM, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoM, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, parents_);

  SOINFO_MEMBER_WRAP(soinfoM, file_offset_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, rtld_flags_);
  SOINFO_MEMBER_REF_WRAP(soinfoM, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoM, strtab_size_);

  SOINFO_MEMBER_WRAP(soinfoM, gnu_nbucket_);
  SOINFO_MEMBER_WRAP(soinfoM, gnu_bucket_);
  SOINFO_MEMBER_WRAP(soinfoM, gnu_chain_);
  SOINFO_MEMBER_WRAP(soinfoM, gnu_maskwords_);
  SOINFO_MEMBER_WRAP(soinfoM, gnu_shift2_);
  SOINFO_MEMBER_WRAP(soinfoM, gnu_bloom_filter_);
  SOINFO_MEMBER_CAST_WRAP(soinfoM, local_group_root_, soinfo *);
  SOINFO_MEMBER_WRAP(soinfoM, android_relocs_);
  SOINFO_MEMBER_WRAP(soinfoM, android_relocs_size_);
  SOINFO_MEMBER_WRAP(soinfoM, soname_);
  SOINFO_MEMBER_STRING_TO_CHARP_WRAP(soinfoM, realpath_);
  SOINFO_MEMBER_WRAP(soinfoM, versym_);
  SOINFO_MEMBER_WRAP(soinfoM, verdef_ptr_);
  SOINFO_MEMBER_WRAP(soinfoM, verdef_cnt_);
  SOINFO_MEMBER_WRAP(soinfoM, verneed_ptr_);
  SOINFO_MEMBER_WRAP(soinfoM, verneed_cnt_);
  SOINFO_MEMBER_CAST_WRAP(soinfoM, target_sdk_version_, int);
}

static void InitApiL1() {
  SOINFO_MEMBER_WRAP(soinfoL1, name_);
  SOINFO_MEMBER_WRAP(soinfoL1, phdr);
  SOINFO_MEMBER_WRAP(soinfoL1, phnum);
  SOINFO_MEMBER_WRAP(soinfoL1, base);
  SOINFO_MEMBER_WRAP(soinfoL1, size);
  SOINFO_MEMBER_WRAP(soinfoL1, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoL1, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, flags_);
  SOINFO_MEMBER_WRAP(soinfoL1, strtab_);
  SOINFO_MEMBER_WRAP(soinfoL1, symtab_);
  SOINFO_MEMBER_WRAP(soinfoL1, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoL1, nchain_);
  SOINFO_MEMBER_WRAP(soinfoL1, bucket_);
  SOINFO_MEMBER_WRAP(soinfoL1, chain_);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoL1, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoL1, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoL1, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoL1, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoL1, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL1, init_array_);
  SOINFO_MEMBER_WRAP(soinfoL1, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL1, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoL1, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL1, init_func_);
  SOINFO_MEMBER_WRAP(soinfoL1, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoL1, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoL1, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoL1, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoL1, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoL1, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoL1, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoL1, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoL1, version_);
  SOINFO_MEMBER_WRAP(soinfoL1, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoL1, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoL1, parents_);

  SOINFO_MEMBER_WRAP(soinfoL1, file_offset_);
  SOINFO_MEMBER_REF_VER_WRAP(soinfoL1, rtld_flags_, L);
  SOINFO_MEMBER_NULL(soinfoL1, dt_flags_1_);
  SOINFO_MEMBER_WRAP(soinfoL1, strtab_size_);

  SOINFO_MEMBER_NULL(soinfoL1, gnu_nbucket_);
  SOINFO_MEMBER_NULL(soinfoL1, gnu_bucket_);
  SOINFO_MEMBER_NULL(soinfoL1, gnu_chain_);
  SOINFO_MEMBER_NULL(soinfoL1, gnu_maskwords_);
  SOINFO_MEMBER_NULL(soinfoL1, gnu_shift2_);
  SOINFO_MEMBER_NULL(soinfoL1, gnu_bloom_filter_);
  SOINFO_MEMBER_NULL(soinfoL1, local_group_root_);
  SOINFO_MEMBER_NULL(soinfoL1, android_relocs_);
  SOINFO_MEMBER_NULL(soinfoL1, android_relocs_size_);
  SOINFO_MEMBER_NULL(soinfoL1, soname_);
  funTable.realpath_ = [](soinfo *thiz) {
    return thiz->get_soname();
  };
  SOINFO_MEMBER_NULL(soinfoL1, versym_);
  SOINFO_MEMBER_NULL(soinfoL1, verdef_ptr_);
  SOINFO_MEMBER_NULL(soinfoL1, verdef_cnt_);
  SOINFO_MEMBER_NULL(soinfoL1, verneed_ptr_);
  SOINFO_MEMBER_NULL(soinfoL1, verneed_cnt_);
  SOINFO_MEMBER_NULL(soinfoL1, target_sdk_version_);
}

static void InitApiL() {
  SOINFO_MEMBER_WRAP(soinfoL, name_);
  SOINFO_MEMBER_WRAP(soinfoL, phdr);
  SOINFO_MEMBER_WRAP(soinfoL, phnum);
  SOINFO_MEMBER_WRAP(soinfoL, base);
  SOINFO_MEMBER_WRAP(soinfoL, size);
  SOINFO_MEMBER_WRAP(soinfoL, dynamic);
  SOINFO_MEMBER_CAST_WRAP(soinfoL, next, soinfo *);
  SOINFO_MEMBER_REF_WRAP(soinfoL, flags_);
  SOINFO_MEMBER_WRAP(soinfoL, strtab_);
  SOINFO_MEMBER_WRAP(soinfoL, symtab_);
  SOINFO_MEMBER_WRAP(soinfoL, nbucket_);
  SOINFO_MEMBER_WRAP(soinfoL, nchain_);
  SOINFO_MEMBER_WRAP(soinfoL, bucket_);
  SOINFO_MEMBER_WRAP(soinfoL, chain_);

#ifndef __LP64__
  // __ANDROID_API_R__   unused4
  SOINFO_MEMBER_WRAP(soinfoL, plt_got_);
#endif
#ifdef USE_RELA
  SOINFO_MEMBER_REF_WRAP(soinfoL, plt_rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, plt_rela_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, rela_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, rela_count_);
#else
  SOINFO_MEMBER_REF_WRAP(soinfoL, plt_rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, plt_rel_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, rel_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, rel_count_);
#endif
  SOINFO_MEMBER_WRAP(soinfoL, preinit_array_);
  SOINFO_MEMBER_WRAP(soinfoL, preinit_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL, init_array_);
  SOINFO_MEMBER_WRAP(soinfoL, init_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL, fini_array_);
  SOINFO_MEMBER_WRAP(soinfoL, fini_array_count_);
  SOINFO_MEMBER_WRAP(soinfoL, init_func_);
  SOINFO_MEMBER_WRAP(soinfoL, fini_func_);

#ifdef __arm__
  SOINFO_MEMBER_WRAP(soinfoL, ARM_exidx);
  SOINFO_MEMBER_WRAP(soinfoL, ARM_exidx_count);
#endif
  SOINFO_MEMBER_WRAP(soinfoL, ref_count_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, link_map_head);
  SOINFO_MEMBER_WRAP(soinfoL, constructors_called);
  SOINFO_MEMBER_WRAP(soinfoL, load_bias);

#ifndef __LP64__
  SOINFO_MEMBER_WRAP(soinfoL, has_text_relocations);
#endif
  SOINFO_MEMBER_WRAP(soinfoL, has_DT_SYMBOLIC);
  SOINFO_MEMBER_WRAP(soinfoL, version_);
  SOINFO_MEMBER_WRAP(soinfoL, st_dev_);
  SOINFO_MEMBER_WRAP(soinfoL, st_ino_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, children_);
  SOINFO_MEMBER_REF_WRAP(soinfoL, parents_);

  SOINFO_MEMBER_NULL(soinfoL, file_offset_);
  SOINFO_MEMBER_NULL(soinfoL, rtld_flags_, L);
  SOINFO_MEMBER_NULL(soinfoL, dt_flags_1_);
  SOINFO_MEMBER_NULL(soinfoL, strtab_size_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_nbucket_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_bucket_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_chain_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_maskwords_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_shift2_);
  SOINFO_MEMBER_NULL(soinfoL, gnu_bloom_filter_);
  SOINFO_MEMBER_NULL(soinfoL, local_group_root_);
  SOINFO_MEMBER_NULL(soinfoL, android_relocs_);
  SOINFO_MEMBER_NULL(soinfoL, android_relocs_size_);
  SOINFO_MEMBER_NULL(soinfoL, soname_);
  funTable.realpath_ = [](soinfo *thiz) {
    return thiz->get_soname();
  };
  SOINFO_MEMBER_NULL(soinfoL, versym_);
  SOINFO_MEMBER_NULL(soinfoL, verdef_ptr_);
  SOINFO_MEMBER_NULL(soinfoL, verdef_cnt_);
  SOINFO_MEMBER_NULL(soinfoL, verneed_ptr_);
  SOINFO_MEMBER_NULL(soinfoL, verneed_cnt_);
  SOINFO_MEMBER_NULL(soinfoL, target_sdk_version_);
}

static void InitCommon() {
  funTable.get_soname = [](soinfo *thiz) -> const char * {
    if (android_api >= __ANDROID_API_M__) {
      return thiz->soname();
    }
    return funTable.name_(thiz);
  };
}

void soinfo::Init() {
#if (defined(__arm__) || defined(__aarch64__))
  useGnuHashNeon = android_api >= __ANDROID_API_R__;
#else
  useGnuHashNeon = false;
#endif

  if (android_api >= __ANDROID_API_T__) {
    InitApiT();
  } else if (android_api >= __ANDROID_API_S__) {
    InitApiS();
  } else if (android_api >= __ANDROID_API_Q__) {
    InitApiQ();
  } else if (android_api >= __ANDROID_API_P__) {
    InitApiP();
  } else if (android_api >= __ANDROID_API_O__) {
    InitApiO();
  } else if (android_api >= __ANDROID_API_N_MR1__) {
    InitApiN1();
  } else if (android_api >= __ANDROID_API_N__) {
    InitApiN();
  } else if (android_api >= __ANDROID_API_M__) {
    InitApiM();
  } else if (android_api >= __ANDROID_API_L_MR1__) {
    InitApiL1();
  } else {
    InitApiL();
  }
  InitCommon();
}

#define DECL_SOINFO_CALL(Name)                                                                                         \
  return_type_trait<member_type_trait<decltype(&SoinfoFunTable::Name)>::type>::type soinfo::Name() {                   \
    return funTable.Name(this);                                                                                        \
  }

#define DECL_SOINFO_CALL_(Name)                                                                                        \
  return_type_trait<member_type_trait<decltype(&SoinfoFunTable::Name##_)>::type>::type soinfo::Name() {                \
    return funTable.Name##_(this);                                                                                     \
  }

DECL_SOINFO_CALL(phdr);

DECL_SOINFO_CALL(phnum);

DECL_SOINFO_CALL(base);

DECL_SOINFO_CALL(size);

DECL_SOINFO_CALL(dynamic);

DECL_SOINFO_CALL(next);

DECL_SOINFO_CALL_(flags);

DECL_SOINFO_CALL_(strtab);

DECL_SOINFO_CALL_(symtab);

DECL_SOINFO_CALL_(nbucket);

DECL_SOINFO_CALL_(nchain);

DECL_SOINFO_CALL_(bucket);

DECL_SOINFO_CALL(bucket_M);

DECL_SOINFO_CALL_(chain);

DECL_SOINFO_CALL(chain_M);

#ifndef __LP64__

DECL_SOINFO_CALL_(plt_got);

#endif

#ifdef USE_RELA
DECL_SOINFO_CALL_(plt_rela);
DECL_SOINFO_CALL_(plt_rela_count);
DECL_SOINFO_CALL_(rela);
DECL_SOINFO_CALL_(rela_count);
#else

DECL_SOINFO_CALL_(plt_rel);

DECL_SOINFO_CALL_(plt_rel_count);

DECL_SOINFO_CALL_(rel);

DECL_SOINFO_CALL_(rel_count);
#endif

DECL_SOINFO_CALL_(preinit_array);

DECL_SOINFO_CALL(preinit_array_P);

DECL_SOINFO_CALL_(preinit_array_count);

DECL_SOINFO_CALL_(init_array);

DECL_SOINFO_CALL(init_array_P);

DECL_SOINFO_CALL_(init_array_count);

DECL_SOINFO_CALL_(fini_array);

DECL_SOINFO_CALL(fini_array_P);

DECL_SOINFO_CALL_(fini_array_count);

DECL_SOINFO_CALL_(init_func);

DECL_SOINFO_CALL(init_func_P);

DECL_SOINFO_CALL_(fini_func);

DECL_SOINFO_CALL(fini_func_P);

#ifdef __arm__
DECL_SOINFO_CALL(ARM_exidx);
DECL_SOINFO_CALL(ARM_exidx_count);
#endif

DECL_SOINFO_CALL_(ref_count);

DECL_SOINFO_CALL(link_map_head);

DECL_SOINFO_CALL(constructors_called);

DECL_SOINFO_CALL(load_bias);

#ifndef __LP64__

DECL_SOINFO_CALL(has_text_relocations);
#endif

DECL_SOINFO_CALL(has_DT_SYMBOLIC);

DECL_SOINFO_CALL_(version);

DECL_SOINFO_CALL_(st_dev);

DECL_SOINFO_CALL_(st_ino);

DECL_SOINFO_CALL_(file_offset);

DECL_SOINFO_CALL_(rtld_flags);

DECL_SOINFO_CALL(rtld_flags_L);

DECL_SOINFO_CALL_(dt_flags_1);

DECL_SOINFO_CALL_(strtab_size);

DECL_SOINFO_CALL_(gnu_nbucket);

DECL_SOINFO_CALL_(gnu_bucket);

DECL_SOINFO_CALL_(gnu_chain);

DECL_SOINFO_CALL_(gnu_maskwords);

DECL_SOINFO_CALL_(gnu_shift2);

DECL_SOINFO_CALL_(gnu_bloom_filter);

DECL_SOINFO_CALL_(local_group_root);

DECL_SOINFO_CALL_(android_relocs);

DECL_SOINFO_CALL_(android_relocs_size);

DECL_SOINFO_CALL_(soname);

DECL_SOINFO_CALL_(realpath);

DECL_SOINFO_CALL_(versym);

DECL_SOINFO_CALL_(verdef_ptr);

DECL_SOINFO_CALL_(verdef_cnt);

DECL_SOINFO_CALL_(verneed_ptr);

DECL_SOINFO_CALL_(verneed_cnt);

DECL_SOINFO_CALL_(target_sdk_version);

DECL_SOINFO_CALL_(dt_runpath);

DECL_SOINFO_CALL_(primary_namespace);

DECL_SOINFO_CALL_(handle);

DECL_SOINFO_CALL_(relr);

DECL_SOINFO_CALL_(relr_count);

DECL_SOINFO_CALL_(tls);

DECL_SOINFO_CALL_(tlsdesc_args);

DECL_SOINFO_CALL_(gap_start);

DECL_SOINFO_CALL_(gap_size);

DECL_SOINFO_CALL(get_soname);

#undef DECL_SOINFO_CALL
#undef DECL_SOINFO_CALL_

//****************************************/

ElfW(Addr) soinfo::resolve_symbol_address(const ElfW(Sym) * s) {
  if (ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC) {
    return call_ifunc_resolver(s->st_value + load_bias());
  }
  return static_cast<ElfW(Addr)>(s->st_value + load_bias());
}

bool soinfo::has_min_version(uint32_t min_version) {
#ifdef __work_around_b_24465209__
  return (flags() & FLAG_NEW_SOINFO) != 0 && version() >= min_version;
#else
  if (android_api >= __ANDROID_API_M__) {
    return true;
  }
  return (flags() & FLAG_NEW_SOINFO) != 0 && version() >= min_version;
#endif
}

ANDROID_GE_M const ElfW(Versym) * soinfo::get_versym_table() { return has_min_version(2) ? versym() : nullptr; }

const char *soinfo::get_string(ElfW(Word) index) {
  if (has_min_version(1)) {
    if (android_api >= __ANDROID_API_L_MR1__) {
      if (index >= strtab_size()) {
        async_safe_fatal("%s: strtab out of bounds error; STRSZ=%zd, name=%d", realpath(), strtab_size(), index);
      }
    }
  }
  return strtab() + index;
}

void soinfo::add_child(soinfo *child) {
  if (has_min_version(0)) {
    get_parents().push_back(this);
    get_parents().push_back(child);
  }
}

soinfo_list_t_wrapper soinfo::get_children() {
  void *ref;
  if (android_api >= __ANDROID_API_T__) {
    ref = &funTable.children_T(this);
  } else {
    ref = &funTable.children_(this);
  }
  return {ref};
}

soinfo_list_t_wrapper soinfo::get_parents() {
  void *ref;
  if (android_api >= __ANDROID_API_T__) {
    ref = &funTable.parents_T(this);
  } else {
    ref = &funTable.parents_(this);
  }
  return {ref};
}

uint32_t soinfo::get_rtld_flags() {
  if (android_api >= __ANDROID_API_M__) {
    return rtld_flags();
  }
  return rtld_flags_L();
}

void soinfo::set_dt_flags_1(uint32_t flag) {
  if (has_min_version(1)) {
    // Android 7.0以下添加进全局组后拥有 RTLD_GLOBAL
    // 标志,这会导致在全局库中使用dlsym无法查找到符号
    if (android_api >= __ANDROID_API_N__) {
      if ((dt_flags_1() & DF_1_GLOBAL) != 0) {
        rtld_flags() |= RTLD_GLOBAL;
      }
    }
    if ((dt_flags_1() & DF_1_NODELETE) != 0) {
      rtld_flags() |= RTLD_NODELETE;
    }
    dt_flags_1() = flag;
  }
}

bool soinfo::is_linked() { return (flags() & FLAG_LINKED) != 0; }

void soinfo::set_linked() {
  flags() |= FLAG_LINKED;
  flags() |= FLAG_IMAGE_LINKED;
}

void soinfo::set_unlinked() {
  flags() &= ~FLAG_LINKED;
  flags() &= ~FLAG_IMAGE_LINKED;
}

bool soinfo::is_gnu_hash() { return (flags() & FLAG_GNU_HASH) != 0; }

void *soinfo::find_export_symbol_address(const char *name) {
  SymbolName find(name);
  const ElfW(Sym) *sym = find_export_symbol_by_name(find, nullptr);
  if (sym) {
    return reinterpret_cast<void *>(resolve_symbol_address(sym));
  }
  return nullptr;
}

const ElfW(Sym) * soinfo::find_export_symbol_by_name(SymbolName &symbol_name, const version_info *vi) {
  return is_gnu_hash() ? gnu_lookup(symbol_name, vi) : elf_lookup(symbol_name, vi);
}

void *soinfo::find_import_symbol_address(const char *name) {
#ifdef USE_RELA
  const ElfW(Rela) *rel = find_import_symbol_by_name(name);
#else
  const ElfW(Rel) *rel = find_import_symbol_by_name(name);
#endif
  return rel ? reinterpret_cast<void *>(rel->r_offset + load_bias()) : nullptr;
}

#ifdef USE_RELA
const ElfW(Rela) * soinfo::find_import_symbol_by_name(const char *name) {
  bool jump;
  int index = find_import_symbol_index_by_name(this, name, jump);
  if (index == -1) {
    return nullptr;
  }
  return jump ? &plt_rela()[index] : &rela()[index];
}
#else

const ElfW(Rel) * soinfo::find_import_symbol_by_name(const char *name) {
  bool jump;
  int index = find_import_symbol_index_by_name(this, name, jump);
  if (index == -1) {
    return nullptr;
  }
  return jump ? &plt_rel()[index] : &rel()[index];
}

#endif

const ElfW(Sym) * soinfo::gnu_lookup(SymbolName &symbol_name, const version_info *vi) {
  if (android_api >= __ANDROID_API_M__) {
    const uint32_t hash = symbol_name.gnu_hash();
    constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
    const uint32_t word_num = (hash / kBloomMaskBits) & gnu_maskwords();
    const ElfW(Addr) bloom_word = gnu_bloom_filter()[word_num];
    const uint32_t h1 = hash % kBloomMaskBits;
    const uint32_t h2 = (hash >> gnu_shift2()) % kBloomMaskBits;

    if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) == 0) {
      return nullptr;
    }

    uint32_t n = gnu_bucket()[hash % gnu_nbucket()];
    if (n == 0) {
      return nullptr;
    }
    const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
    const ElfW(Versym) *versym = get_versym_table();

    do {
      ElfW(Sym) *s = symtab() + n;
      if (((gnu_chain()[n] ^ hash) >> 1) == 0 && check_symbol_version(versym, n, verneed) &&
          strcmp(get_string(s->st_name), symbol_name.get_name()) == 0
          /*&& is_symbol_global_and_defined(this, s)*/ // 查找符号不区分全局还是局部
      ) {
        return symtab() + n;
      }
    } while ((gnu_chain()[n++] & 1) == 0);
    return nullptr;
  }

  return elf_lookup(symbol_name, vi);
}

const ElfW(Sym) * soinfo::elf_lookup(SymbolName &symbol_name, const version_info *vi) {
  uint32_t hash = symbol_name.elf_hash();
  const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
  const ElfW(Versym) *versym = get_versym_table();

  if (android_api >= 23) {
    for (uint32_t n = bucket_M()[hash % nbucket()]; n != 0; n = chain_M()[n]) {
      ElfW(Sym) *s = symtab() + n;

      if (check_symbol_version(versym, n, verneed) && strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
          is_symbol_global_and_defined(this, s)) {
        return symtab() + n;
      }
    }
  } else {
    for (uint32_t n = bucket()[hash % nbucket()]; n != 0; n = chain()[n]) {
      ElfW(Sym) *s = symtab() + n;

      if (check_symbol_version(versym, n, verneed) && strcmp(get_string(s->st_name), symbol_name.get_name()) == 0 &&
          is_symbol_global_and_defined(this, s)) {
        return symtab() + n;
      }
    }
  }

  return nullptr;
}

std::pair<uint32_t, uint32_t> soinfo::get_export_symbol_gnu_table_size() {
  if (is_gnu_hash()) {
    if (android_api >= __ANDROID_API_M__) {
      int max = gnu_bucket()[gnu_nbucket() - 1];
      while ((gnu_chain()[max] & 1) == 0) {
        max++;
      }
      return {gnu_bucket()[0], max};
    }
  }
  return {1, 0};
}

size_t soinfo::get_symbols_count() {
  if (is_gnu_hash()) {
    return get_export_symbol_gnu_table_size().second + 1;
  }
  return nchain() + 1;
}

symbol_relocations soinfo::get_global_soinfo_export_symbols() {
  uint32_t start = 0;
  uint32_t end;
  ElfW(Sym) sym;
  size_t len = 0;
  symbol_relocations result;

  if (is_gnu_hash()) {
    // gnu查找符号数量是chain中最后一个数 & 1 == 1
    // max(bucket)while ((chain[ix - symoffset] & 1) == 0) ix++;
    std::tie(start, end) = get_export_symbol_gnu_table_size();
  } else {
    // elf hash 表没有对内部符号和外部符号排序
    end = nchain();
  }

  auto find_global = [](ElfW(Sym) & s) {
    return s.st_shndx != SHN_UNDEF && ELF_ST_BIND(s.st_info) == STB_GLOBAL && ELF_ST_TYPE(s.st_info) == STT_FUNC;
  };
  if (end < start) {
    return result;
  }
  for (uint32_t i = start; i <= end; ++i) {
    sym = symtab()[i];
    if (!find_global(sym)) {
      continue;
    }
    result.emplace_back(symbol_relocation{get_string(sym.st_name), resolve_symbol_address(&sym), this});
    len++;
  }
  return result;
}

symbol_relocations soinfo::get_global_soinfo_export_symbols(const std::vector<std::string> &filters) {
  symbol_relocations symbols = get_global_soinfo_export_symbols();
  if (symbols.empty() || filters.empty()) {
    return symbols;
  }
  symbols.erase(std::remove_if(symbols.begin(), symbols.end(),
                               [&](symbol_relocation &sym) {
                                 return std::find(filters.begin(), filters.end(), sym.name) != filters.end();
                               }),
                symbols.end());
  return symbols;
}

bool soinfo::again_process_relocation(symbol_relocations &rels) {
  fakelinker::MapsHelper util;
  if (!util.GetLibraryProtect(get_soname())) {
    LOGE("No access to the library: %s", get_soname() == nullptr ? "(null)" : get_soname());
    return false;
  }
  if (!util.UnlockPageProtect()) {
    LOGE("cannot change soinfo: %s memory protect", get_soname() == nullptr ? "(null)" : get_soname());
    return false;
  }
  LOGV("again relocation library: %s", get_soname());

#if defined(USE_RELA)
  if (rela() != nullptr) {
    plain_relocate_impl<RelocMode::Typical>(this, rela(), rela_count(), rels);
  }
  if (plt_rela() != nullptr) {
    plain_relocate_impl<RelocMode::JumpTable>(this, plt_rela(), plt_rela_count(), rels);
  }
#else
  if (rel() != nullptr) {
    plain_relocate_impl<RelocMode::Typical>(this, rel(), rel_count(), rels);
  }
  if (plt_rel() != nullptr) {
    plain_relocate_impl<RelocMode::JumpTable>(this, plt_rel(), plt_rel_count(), rels);
  }
#endif
  util.RecoveryPageProtect();
  return true;
}

ElfW(Addr) soinfo::get_verdef_ptr() {
  CHECK(android_api >= __ANDROID_API_M__);
  return verdef_ptr();
}

size_t soinfo::get_verdef_cnt() {
  CHECK(android_api >= __ANDROID_API_M__);
  return verdef_cnt();
}

ANDROID_GE_R SymbolLookupLib soinfo::get_lookup_lib() {
  SymbolLookupLib result{};
  result.si_ = this;

  // For libs that only have SysV hashes, leave the gnu_bloom_filter_ field NULL
  // to signal that the fallback code path is needed.
  if (!is_gnu_hash()) {
    return result;
  }
  result.gnu_maskwords_ = gnu_maskwords();
  result.gnu_shift2_ = gnu_shift2();
  result.gnu_bloom_filter_ = gnu_bloom_filter();
  result.strtab_ = strtab();
  result.strtab_size_ = strtab_size();
  result.symtab_ = symtab();
  result.versym_ = get_versym_table();
  result.gnu_chain_ = gnu_chain();
  result.gnu_nbucket_ = gnu_nbucket();
  result.gnu_bucket_ = gnu_bucket();
  return result;
}

ANDROID_GE_N android_namespace_t *&soinfo::get_primary_namespace() { return primary_namespace(); }

ANDROID_GE_N void soinfo::add_secondary_namespace(android_namespace_t *secondary_ns) {
  LinkerBlockLock lock;
  get_secondary_namespaces().push_back(secondary_ns);
}

ANDROID_GE_N void soinfo::remove_secondary_namespace(android_namespace_t *secondary_ns) {
  LinkerBlockLock lock;
  get_secondary_namespaces().remove_if([&](android_namespace_t *np) {
    return secondary_ns == np;
  });
}

ANDROID_GE_N void soinfo::remove_all_secondary_namespace() { get_secondary_namespaces().clear(); }

ANDROID_GE_N android_namespace_list_t_wrapper soinfo::get_secondary_namespaces() {
  void *ref;
  if (android_api >= __ANDROID_API_T__) {
    ref = &funTable.secondary_namespaces_T(this);
  } else {
    ref = &funTable.secondary_namespaces_(this);
  }
  return {ref};
}

ANDROID_GE_N uintptr_t soinfo::get_handle() { return handle(); }

ElfW(Addr) soinfo::call_ifunc_resolver(ElfW(Addr) resolver_addr) { return __bionic_call_ifunc_resolver(resolver_addr); }

#undef SOINFO_FUN
#undef SOINFO_CONST_FUN