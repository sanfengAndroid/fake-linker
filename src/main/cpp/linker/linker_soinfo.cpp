//
// Created by beich on 2020/11/5.
//

#include <dlfcn.h>
#include <sys/auxv.h>

#include <maps_util.h>

#include "linker_soinfo.h"
#include "linker_globals.h"
#include "linker_util.h"
#include "linker_relocs.h"
#include "local_block_allocator.h"

#if (defined(__arm__) || defined(__aarch64__)) && __ANDROID_API__ >= __ANDROID_API_R__
#define USE_GNU_HASH_NEON 1
#else
#define USE_GNU_HASH_NEON 0
#endif

#if USE_GNU_HASH_NEON
#include "linker_gnu_hash_neon.h"
#endif

#define LINKER_VERBOSITY_PRINT (-1)
#define LINKER_VERBOSITY_INFO   0
#define LINKER_VERBOSITY_TRACE  1
#define LINKER_VERBOSITY_DEBUG  2

#define LINKER_DEBUG_TO_LOG  1

#define TRACE_DEBUG          1
#define DO_TRACE_LOOKUP      1
#define DO_TRACE_RELO        1
#define DO_TRACE_IFUNC       1
#define TIMING               0
#define STATS                0

constexpr ElfW(Versym) kVersymNotNeeded = 0;
constexpr ElfW(Versym) kVersymGlobal = 1;
constexpr ElfW(Versym) kVersymHiddenBit = 0x8000;

#if defined(__aarch64__)
/**
 * Provides information about hardware capabilities to ifunc resolvers.
 *
 * Starting with API level 30, ifunc resolvers on arm64 are passed two arguments. The first is a
 * uint64_t whose value is equal to getauxval(AT_HWCAP) | _IFUNC_ARG_HWCAP. The second is a pointer
 * to a data structure of this type. Prior to API level 30, no arguments are passed to ifunc
 * resolvers. Code that wishes to be compatible with prior API levels should not accept any
 * arguments in the resolver.
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
 * If this bit is set in the first argument to an ifunc resolver, indicates that the second argument
 * is a pointer to a data structure of type __ifunc_arg_t. This bit is always set on Android
 * starting with API level 30.
 */
#define _IFUNC_ARG_HWCAP (1ULL << 62)
#endif


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
    typedef ElfW(Addr) (*ifunc_resolver_t)(void);
    return reinterpret_cast<ifunc_resolver_t>(resolver_addr)();
#endif
}

static inline bool check_symbol_version(const ElfW(Versym) *ver_table, uint32_t sym_idx,
                                        const ElfW(Versym) verneed) {
    if (ver_table == nullptr) return true;
    const uint32_t verdef = ver_table[sym_idx];
    return (verneed == kVersymNotNeeded) ? !(verdef & kVersymHiddenBit) : verneed == (verdef & ~kVersymHiddenBit);
}

inline bool is_symbol_global_and_defined(const soinfo *si, const ElfW(Sym) *s) {
    if (ELF_ST_BIND(s->st_info) == STB_GLOBAL || ELF_ST_BIND(s->st_info) == STB_WEAK) {
        return s->st_shndx != SHN_UNDEF;
    } else if (ELF_ST_BIND(s->st_info) != STB_LOCAL) {
        LOGW("Warning: unexpected ST_BIND value: %d for \"%s\" in \"%s\" (ignoring)",
             ELF_ST_BIND(s->st_info), si->get_string(s->st_name), si->get_realpath());
    }
    return false;
}


ElfW(Addr) soinfo::get_verdef_ptr() const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    return verdef_ptr_;
#endif
    return 0;
}

size_t soinfo::get_verdef_cnt() const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    return verdef_cnt_;
#endif
    return 0;
}

template<typename F>
static bool for_each_verdef(const soinfo *si, F functor) {
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
            LOGE("unsupported verdef[%zd] vd_version: %d (expected 1) library: %s", i, verdef->vd_version,
                 si->get_realpath());
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

ElfW(Versym) find_verdef_version_index(const soinfo *si, const version_info *vi) {
    if (vi == nullptr) {
        return kVersymNotNeeded;
    }

    ElfW(Versym) result = kVersymGlobal;

    if (!for_each_verdef(si, [&](size_t, const ElfW(Verdef) *verdef, const ElfW(Verdaux) *verdaux) {
                             if (verdef->vd_hash == vi->elf_hash &&
                                 strcmp(vi->name, si->get_string(verdaux->vda_name)) == 0) {
                                 result = verdef->vd_ndx;
                                 return true;
                             }
                             return false;
                         }
    )) {
        CHECK_OUTPUT(false, "invalid verdef after prelinking: %s", si->get_realpath());
    }
    return result;
}

static bool is_lookup_tracing_enabled() {
    if (ProxyLinker::GetGLdDebugVerbosity() == nullptr) {
        return false;
    }
    return *ProxyLinker::GetGLdDebugVerbosity() > LINKER_VERBOSITY_TRACE && DO_TRACE_LOOKUP;
}

const char *soinfo::get_soname() const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    return soname_;
#else
    return name_;
#endif
}

ElfW(Addr) soinfo::resolve_symbol_address(const ElfW(Sym) *s) const {
    if (ELF_ST_TYPE(s->st_info) == STT_GNU_IFUNC) {
        return call_ifunc_resolver(s->st_value + load_bias);
    }
    return static_cast<ElfW(Addr)>(s->st_value + load_bias);
}

const char *soinfo::get_string(ElfW(Word) index) const {
    if (has_min_version(1)) {
#if __ANDROID_API__ >= __ANDROID_API_L_MR1__
        if (index >= strtab_size_) {
            async_safe_fatal("%s: strtab out of bounds error; STRSZ=%zd, name=%d", get_realpath(), strtab_size_, index);
        }
#endif
    }
    return strtab_ + index;
}

const char *soinfo::get_realpath() const {
#if __ANDROID_API__ <= __ANDROID_API_L_MR1__
    return name_;
#elif defined(__work_around_b_24465209__)
    if (has_min_version(2)) {
        return realpath_.c_str();
    } else {
        return old_name_;
    }
#else
    return realpath_.c_str();
#endif
}

const ElfW(Versym) *soinfo::get_versym_table() const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    return has_min_version(2) ? versym_ : nullptr;
#else
    return nullptr;
#endif
}

ElfW(Addr) call_ifunc_resolver(ElfW(Addr) resolver_addr) {
    ElfW(Addr) ifunc_addr = __bionic_call_ifunc_resolver(resolver_addr);
    return ifunc_addr;
}

void soinfo::add_child(soinfo *child) {
    if (has_min_version(0)) {
        child->parents_.push_back(this);
        this->children_.push_back(child);
    }
}

const soinfo_list_t &soinfo::get_children() const {
    return children_;
}

soinfo_list_t &soinfo::get_parents() {
    return parents_;
}

#if __ANDROID_API__ >= __ANDROID_API_L_MR1__

uint32_t soinfo::get_rtld_flags() const {
    return rtld_flags_;
}

#endif

uint32_t soinfo::get_dt_flags_1() const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    if (has_min_version(1)) {
        return dt_flags_1_;
    }
#endif
    return 0;
}

#if __ANDROID_API__ >= __ANDROID_API_M__

void soinfo::set_dt_flags_1(uint32_t dt_flags_1) {

    if (has_min_version(1)) {
//		Android 7.0以下添加进全局组后拥有 RTLD_GLOBAL标志,这会导致在全局库中使用dlsym无法查找到符号
#if __ANDROID_API__ >= __ANDROID_API_N__
        if ((dt_flags_1 & DF_1_GLOBAL) != 0) {
            rtld_flags_ |= RTLD_GLOBAL;
        }
#endif
        if ((dt_flags_1 & DF_1_NODELETE) != 0) {
            rtld_flags_ |= RTLD_NODELETE;
        }
        dt_flags_1_ = dt_flags_1;
    }
}

#endif

bool soinfo::is_linked() const {
    return (flags_ & FLAG_LINKED) != 0;
}

void soinfo::set_linked() {
    flags_ |= FLAG_LINKED;
    flags_ |= FLAG_IMAGE_LINKED;
}

void soinfo::set_unlinked() {
    flags_ &= ~FLAG_LINKED;
    flags_ &= ~FLAG_IMAGE_LINKED;
}

void *soinfo::find_export_symbol_address(const char *name) const {
    SymbolName find(name);
    const ElfW(Sym) *sym = find_export_symbol_by_name(find, nullptr);
    if (sym) {
        return reinterpret_cast<void *>(resolve_symbol_address(sym));
    }
    return nullptr;
}

void *soinfo::find_import_symbol_address(const char *name) const {
#ifdef USE_RELA
    const ElfW(Rela)* rel = find_import_symbol_by_name(name);
#else
    const ElfW(Rel) *rel = find_import_symbol_by_name(name);
#endif
    return rel == nullptr ? nullptr : reinterpret_cast<void *>(rel->r_offset + load_bias);
}

static int find_import_symbol_index_by_name(const soinfo *si, const char *name, bool &type) {
#ifdef USE_RELA
    ElfW(Rela) *start = si->plt_rela_ == nullptr ? si->rela_ : si->plt_rela_;
    ElfW(Rela) *end = si->plt_rela_ == nullptr ? si->rela_ + si->rela_count_ : si->plt_rela_ + si->plt_rela_count_;
    type = si->plt_rela_ != nullptr;
#else
    ElfW(Rel) *start = si->plt_rel_ == nullptr ? si->rel_ : si->plt_rel_;
    ElfW(Rel) *end = si->plt_rel_ == nullptr ? si->rel_ + si->rel_count_ : si->plt_rel_ + si->plt_rel_count_;
    type = si->plt_rel_ != nullptr;
#endif
    int index = 0;
    const char *sym_name;
    for (; start < end; start++, index++) {
        sym_name = si->get_string(si->symtab_[R_SYM(start->r_info)].st_name);
        if (strcmp(sym_name, name) == 0) {
            return index;
        }
    }
    return -1;
}

#ifdef USE_RELA
const ElfW(Rela) *soinfo::find_import_symbol_by_name(const char *name)  const{
    bool jump;
    int index = find_import_symbol_index_by_name(this, name, jump);
    if (index == -1) {
        return nullptr;
    }
    return jump ? &plt_rela_[index] : &rela_[index];
}
#else

const ElfW(Rel) *soinfo::find_import_symbol_by_name(const char *name) const {
    bool jump;
    int index = find_import_symbol_index_by_name(this, name, jump);
    if (index == -1) {
        return nullptr;
    }
    return jump ? &plt_rel_[index] : &rel_[index];
}

#endif


const ElfW(Sym) *soinfo::find_export_symbol_by_name(SymbolName &symbol_name,
                                                    const version_info *vi) const {
    return is_gnu_hash() ? gnu_lookup(symbol_name, vi) : elf_lookup(symbol_name, vi);
}

const ElfW(Sym) *soinfo::gnu_lookup(SymbolName &symbol_name, const version_info *vi) const {
#if __ANDROID_API__ >= __ANDROID_API_M__
    const uint32_t hash = symbol_name.gnu_hash();

    constexpr uint32_t kBloomMaskBits = sizeof(ElfW(Addr)) * 8;
    const uint32_t word_num = (hash / kBloomMaskBits) & gnu_maskwords_;
    const ElfW(Addr) bloom_word = gnu_bloom_filter_[word_num];
    const uint32_t h1 = hash % kBloomMaskBits;
    const uint32_t h2 = (hash >> gnu_shift2_) % kBloomMaskBits;

    if ((1 & (bloom_word >> h1) & (bloom_word >> h2)) == 0) {
        return nullptr;
    }

    uint32_t n = gnu_bucket_[hash % gnu_nbucket_];
    if (n == 0) {
        return nullptr;
    }

    const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
    const ElfW(Versym) *versym = get_versym_table();

    do {
        ElfW(Sym) *s = symtab_ + n;
        if (((gnu_chain_[n] ^ hash) >> 1) == 0 &&
            check_symbol_version(versym, n, verneed) &&
            strcmp(get_string(s->st_name), symbol_name.get_name()) == 0
            /*&& is_symbol_global_and_defined(this, s)*/    // 查找符号不区分全局还是局部
                ) {
            return symtab_ + n;
        }
    } while ((gnu_chain_[n++] & 1) == 0);
    return nullptr;
#else
    return elf_lookup(symbol_name, vi);
#endif
}

const ElfW(Sym) *soinfo::elf_lookup(SymbolName &symbol_name, const version_info *vi) const {
    uint32_t hash = symbol_name.elf_hash();

    const ElfW(Versym) verneed = find_verdef_version_index(this, vi);
    const ElfW(Versym) *versym = get_versym_table();

    for (uint32_t n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
        ElfW(Sym) *s = symtab_ + n;

        if (check_symbol_version(versym, n, verneed) &&
            strcmp(get_string(s->st_name), symbol_name.get_name()) == 0
            && is_symbol_global_and_defined(this, s)) {
            return symtab_ + n;
        }
    }
    return nullptr;
}

#if __ANDROID_API__ >= __ANDROID_API_N__

android_namespace_t *soinfo::get_primary_namespace() {
    return primary_namespace_;
}

void soinfo::add_secondary_namespace(android_namespace_t *secondary_ns) {
    secondary_namespaces_.push_back(secondary_ns);
}

android_namespace_list_t &soinfo::get_secondary_namespaces() {
    return secondary_namespaces_;
}

uintptr_t soinfo::get_handle() const {
    return handle_;
}

#endif

bool soinfo::is_gnu_hash() const {
    return (flags_ & FLAG_GNU_HASH) != 0;
}

#if __ANDROID_API__ >= __ANDROID_API_R__

SymbolLookupLib soinfo::get_lookup_lib() {
    SymbolLookupLib result{};
    result.si_ = this;

    // For libs that only have SysV hashes, leave the gnu_bloom_filter_ field NULL to signal that
    // the fallback code path is needed.
    if (!is_gnu_hash()) {
        return result;
    }
    result.gnu_maskwords_ = gnu_maskwords_;
    result.gnu_shift2_ = gnu_shift2_;
    result.gnu_bloom_filter_ = gnu_bloom_filter_;
    result.strtab_ = strtab_;
    result.strtab_size_ = strtab_size_;
    result.symtab_ = symtab_;
    result.versym_ = get_versym_table();
    result.gnu_chain_ = gnu_chain_;
    result.gnu_nbucket_ = gnu_nbucket_;
    result.gnu_bucket_ = gnu_bucket_;
    return result;
}

SymbolLookupList::SymbolLookupList(soinfo *si)
        : sole_lib_(si->get_lookup_lib()), begin_(&sole_lib_), end_(&sole_lib_ + 1) {
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

#endif

uint32_t SymbolName::elf_hash() {
    if (!has_elf_hash_) {
        elf_hash_ = calculate_elf_hash(name_);
        has_elf_hash_ = true;
    }

    return elf_hash_;
}

uint32_t SymbolName::gnu_hash() {
    if (!has_gnu_hash_) {
#if USE_GNU_HASH_NEON
        gnu_hash_ = calculate_gnu_hash_neon(name_).first;
        has_gnu_hash_ = true;
#else
        gnu_hash_ = calculate_gnu_hash(name_);
        has_gnu_hash_ = true;
#endif
    }
    return gnu_hash_;
}

enum class RelocMode {
    // Fast path for JUMP_SLOT relocations.
    JumpTable,
    // Fast path for typical relocations: ABSOLUTE, GLOB_DAT, or RELATIVE.
    Typical,
    // Handle all relocation types, relocations in text sections, and statistics/tracing.
    General,
};

template<RelocMode Mode>
static bool process_relocation(soinfo *so, const rel_t &reloc, symbol_relocations *rels) {
    // rel 通用
    void *const rel_target = reinterpret_cast<void *>(reloc.r_offset + so->load_bias);
    const uint32_t r_type = R_TYPE(reloc.r_info);
    const uint32_t r_sym = R_SYM(reloc.r_info);
    const char *sym_name = r_sym != 0 ? so->get_string(so->symtab_[r_sym].st_name) : nullptr;
    ElfW(Addr) sym_addr = 0;

    if (__predict_false(r_type == R_GENERIC_NONE)) {// R_GENERIC_NONE
        return false;
    }
    if (is_tls_reloc(r_type)) {
        return false;
    }
    if (__predict_false(sym_name == nullptr)) {
        return false;
    }
    if (ELF_ST_BIND(so->symtab_[r_sym].st_info) == STB_LOCAL) {
        return false;
    }
#if defined(USE_RELA)
    auto get_addend_rel = [&]() -> ElfW(Addr) { return reloc.r_addend; };
    auto get_addend_norel = [&]() -> ElfW(Addr) { return reloc.r_addend; };
#else
    // 这种情况下已经重定位了就无法再获得该 addend,但是通常我们要重定位的符号用不上
    auto get_addend_rel = [&]() -> ElfW(Addr) {
        LOGE("Error: Symbols that may be wrong are being relocated");
        return *static_cast<ElfW(Addr) *>(rel_target);
    };
    auto get_addend_norel = [&]() -> ElfW(Addr) { return 0; };
#endif
    ElfW(Addr) orig = *static_cast<ElfW(Addr) *>(rel_target);
    for (int i = 0; i < rels->len; ++i) {
        if (strcmp(sym_name, rels->elements[i].name) == 0) {
            sym_addr = rels->elements[i].sym_address;
            if (Mode == RelocMode::JumpTable) {
                if (r_type == R_GENERIC_JUMP_SLOT) {
                    *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_norel();
                    LOGV("Relocation symbol JumpTable: %s, original address: %p, new address: %p", sym_name, reinterpret_cast<void *>(orig),
                         reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
                    return true;
                }
            }
            if (Mode == RelocMode::Typical) {
                if (r_type == R_GENERIC_ABSOLUTE) {
                    *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_rel();
                    LOGV("Relocation symbol Typical ABSOLUTE: %s, original address: %16p,  new address: %16p", sym_name, reinterpret_cast<void *>(orig),
                         reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
                    return true;
                } else if (r_type == R_GENERIC_GLOB_DAT) {
                    *static_cast<ElfW(Addr) *>(rel_target) = sym_addr + get_addend_norel();
                    LOGV("Relocation symbol Typical GLOB_DAT: %s, original address: %16p,  new address: %16p", sym_name, reinterpret_cast<void *>(orig),
                         reinterpret_cast<void *>(*static_cast<ElfW(Addr) *>(rel_target)));
                    return true;
                }
            }
            return false;
        }
    }
    return true;
}

template<RelocMode OptMode>
static bool plain_relocate_impl(soinfo *so, rel_t *rels, size_t rel_count, symbol_relocations *symbols) {
    for (size_t i = 0; i < rel_count; ++i) {
        process_relocation<OptMode>(so, rels[i], symbols);
    }
    return true;
}

std::pair<uint32_t, uint32_t> soinfo::get_export_symbol_gnu_table_size() {
    if (is_gnu_hash()) {
#if __ANDROID_API__ >= __ANDROID_API_M__
        int max = gnu_bucket_[gnu_nbucket_ - 1];
        while ((gnu_chain_[max] & 1) == 0) {
            max++;
        }
        return {gnu_bucket_[0], max};
#endif
    }
    return {0, 0};
}

size_t soinfo::get_symbols_count() {
    if (is_gnu_hash()) {
        return get_export_symbol_gnu_table_size().second + 1;
    } else {
        return nchain_ + 1;
    }
}

symbol_relocations *soinfo::get_global_soinfo_export_symbols() {
    uint32_t start = 0;
    uint32_t end;
    ElfW(Sym) sym;
    symbol_relocation *rels;
    size_t len = 0;
    symbol_relocations *result = nullptr;

    if (is_gnu_hash()) {
        // gnu查找符号数量是chain中最后一个数 & 1 == 1
        // max(bucket)while ((chain[ix - symoffset] & 1) == 0) ix++;
        std::pair<uint32_t, uint32_t> pair = get_export_symbol_gnu_table_size();
        start = pair.first;
        end = pair.second;
    } else {
        // elf hash 表没有对内部符号和外部符号排序
        end = nchain_;
    }

    auto find_global = [](ElfW(Sym) &s) {
        return s.st_shndx != SHN_UNDEF &&
               ELF_ST_BIND(s.st_info) == STB_GLOBAL &&
               ELF_ST_TYPE(s.st_info) == STT_FUNC;
    };
    if (end <= start) {
        return nullptr;
    }
    rels = static_cast<symbol_relocation *>(malloc(sizeof(symbol_relocation) * (end - start + 1)));
    for (int i = start; i <= end; ++i) {
        sym = symtab_[i];
        if (!find_global(sym)) {
            continue;
        }
        rels[len].name = get_string(sym.st_name);
        rels[len].sym_address = resolve_symbol_address(&sym);
        rels[len].source = this;
        len++;
    }
    if (len != 0) {
        result = VarLengthObjectAlloc<symbol_relocation>(len);
        memcpy(result->elements, rels, sizeof(symbol_relocation) * len);
    }
    free(rels);
    return result;
}

symbol_relocations *soinfo::get_global_soinfo_export_symbols(std::vector<std::string> &filters) {
    symbol_relocations *rels = get_global_soinfo_export_symbols();
    if (rels == nullptr || filters.empty()) {
        return rels;
    }
    std::vector<int> removes;

    for (int i = 0; i < rels->len; ++i) {
        std::string name = rels->elements[i].name;
        if (std::find(filters.begin(), filters.end(), name) != filters.end()) {
            removes.push_back(i);
        }
    }
    if (removes.empty()) {
        return rels;
    }
    symbol_relocations *new_rels = VarLengthObjectAlloc<symbol_relocation>(rels->len - removes.size());
    int index = 0;
    for (int i = 0; i < rels->len; ++i) {
        if (std::find(removes.begin(), removes.end(), i) == removes.end()) {
            new_rels->elements[index].name = rels->elements[i].name;
            new_rels->elements[index].sym_address = rels->elements[i].sym_address;
            new_rels->elements[index++].source = rels->elements[i].source;
        }
    }
    return new_rels;
}


bool soinfo::again_process_relocation(symbol_relocations *rels) {
    MapsUtil util;
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
    if (rela_ != nullptr) {
        plain_relocate_impl<RelocMode::Typical>(this, rela_, rela_count_, rels);
    }
    if (plt_rela_ != nullptr) {
        plain_relocate_impl<RelocMode::JumpTable>(this, plt_rela_, plt_rela_count_, rels);
    }
#else
    if (rel_ != nullptr) {
        plain_relocate_impl<RelocMode::Typical>(this, rel_, rel_count_, rels);
    }
    if (plt_rel_ != nullptr) {
        plain_relocate_impl<RelocMode::JumpTable>(this, plt_rel_, plt_rel_count_, rels);
    }
#endif

    util.RecoveryPageProtect();
    return true;
}


//bool VersionTracker::init(const soinfo *si_from) {
//	if (!si_from->has_min_version(2)) {
//		return true;
//	}
//
//	return init_verneed(si_from) && init_verdef(si_from);
//}
//
//const version_info *VersionTracker::get_version_info(ElfW(Versym) source_symver) const {
//	if (source_symver < 2 ||
//		source_symver >= version_infos.size() ||
//		version_infos[source_symver].name == nullptr) {
//		return nullptr;
//	}
//
//	return &version_infos[source_symver];
//}