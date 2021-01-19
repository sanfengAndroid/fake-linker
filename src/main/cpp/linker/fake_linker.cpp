//
// Created by lenovo-s on 2019/4/15.
//

#include "fake_linker.h"
#include "linker_util.h"


#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <link.h>
#include <malloc.h>
#include <cinttypes>

#include <maps_util.h>

typedef struct {
    uint32_t nbucket;
    uint32_t nchain;

    uint32_t *bucket;
    uint32_t *chain;
} Elf32_HashTable;

typedef Elf32_HashTable Elf64_HashTable;

typedef struct {
    uint32_t nbuckets;
    uint32_t symndx;
    uint32_t maskwords;
    uint32_t shift2;

    uint32_t *bloom_filter;
    uint32_t *bucket;
    uint32_t *chain;

} Elf32_GnuHashTable;

typedef struct {
    uint32_t nbuckets;
    uint32_t symndx;
    uint32_t maskwords;
    uint32_t shift2;

    uint64_t *bloom_filter;
    uint32_t *bucket;
    uint32_t *chain;
} Elf64_GnuHashTable;

typedef struct {
    ElfW(Off) file_start_offset;
    ElfW(Addr) vaddr_start;
    ElfW(Word) file_length;
    ElfW(Word) mem_length;
} program_table_area;


typedef struct {
    ElfW(Ehdr) *p_ehdr;
    ElfW(Phdr) *p_phdr;

    ElfW(Dyn) *p_pt_dyn;

    union {
        ElfW(Rel) *p_dt_jmp_rel;
        ElfW(Rela) *p_dt_jmp_rela;
    } dt_jmprel;

    ElfW(Sym) *p_dt_sym;
    char *p_dt_strtab;

    ElfW(HashTable) hash_table;
    ElfW(GnuHashTable) gun_hash_table;

    int dt_pltrel;
    int dt_pltrel_size;
    int pt_dyn_size;
    int dt_strtab_size;
    int dt_sym_size;

    program_table_area *p_ph_areas;

} ie_find_params;

typedef struct {
    gaddress sh_strtab_offset;
    gaddress sh_strtab_addr;    // 实际内存中的地址
    gsize sh_strtab_size;    // 节区大小
    gsize str_addralign;

    gaddress sh_symtab_offset;
    gaddress sh_symtab_addr;
    gsize sh_symtab_size;
    gsize sym_entsize;
    gsize sym_addralign;
    gsize sym_num;
} SymbolParams;

static gaddress FileOffsetToVaddress(program_table_area *areas, int num, gsize offset);

static gaddress HashLookup(ie_find_params *params, const char *name, int *sym_index);

static gaddress GnuHashLookup(ie_find_params *params, const char *name, int *sym_index);

static gaddress DynamicRelFindSymbol(ie_find_params *params, int sym_index);

static gaddress DynamicRelaFindSymbol(ie_find_params *params, int sym_index);

static gaddress ResolveInnerSymbolAddress(const char *library_name, const char *symbol_name);

static int ResolveInnerSymbolsAddress(const char *library_name, symbols_address *ret, const char **symbols);

/*
 * 导入或导出符号会存在动态节区的符号表中,直接内存查找即可
 * 导出符号直接在symtab表中就包含了地址,而导入符号需要经过DT_JMPREL才能找到实际地址
 *
 * */
gaddress ResolveLibrarySymbolAddress(const char *library_name, SymbolType type, const char *symbol_name) {
    gaddress base;
    ie_find_params params = {nullptr};
    gaddress tmp = 0;
    gaddress retval = 0;
    int sym_index;

    if (type == kInner) {
        return ResolveInnerSymbolAddress(library_name, symbol_name);
    }
    MapsUtil util;
    base = util.FindLibraryBase(library_name, "r");
    if (base == 0) {
        goto end;
    }

    params.p_ehdr = (ElfW(Ehdr) *) GSIZE_TO_POINTER(base);
    params.p_ph_areas = static_cast<program_table_area *>(malloc(sizeof(program_table_area) * params.p_ehdr->e_phnum));
    params.p_phdr = (ElfW(Phdr) *) (base + params.p_ehdr->e_phoff);

    for (int i = 0; i < params.p_ehdr->e_phnum; ++i, params.p_phdr++) {
        params.p_ph_areas[i].file_start_offset = params.p_phdr->p_offset;
        params.p_ph_areas[i].vaddr_start = params.p_phdr->p_vaddr;
        params.p_ph_areas[i].file_length = params.p_phdr->p_filesz;
        params.p_ph_areas[i].mem_length = params.p_phdr->p_memsz;

        if (params.p_phdr->p_type == PT_DYNAMIC) {
            params.p_pt_dyn = (ElfW(Dyn) *) GSIZE_TO_POINTER(base + params.p_phdr->p_vaddr);
            params.pt_dyn_size = params.p_phdr->p_filesz / sizeof(ElfW(Dyn));
        }
    }

    if (params.p_pt_dyn == nullptr) {
        LOGE("not found dynamic segment, library: %s", library_name);
        goto end;
    }

    for (int i = 0; i < params.pt_dyn_size; ++i, params.p_pt_dyn++) {
        switch (params.p_pt_dyn->d_tag) {
            case DT_SYMTAB:
                tmp = FileOffsetToVaddress(params.p_ph_areas, params.p_ehdr->e_phnum,
                                           params.p_pt_dyn->d_un.d_ptr);
                if (tmp == 0) {
                    LOGE("Failed to find symbol table(DT_SYMTAB), library: %s", library_name);
                    goto end;
                }
                params.p_dt_sym = (ElfW(Sym) *) (base + tmp);
                break;
            case DT_SYMENT:
                params.dt_sym_size = params.p_pt_dyn->d_un.d_val;
                break;
            case DT_STRTAB:
                tmp = FileOffsetToVaddress(params.p_ph_areas, params.p_ehdr->e_phnum,
                                           params.p_pt_dyn->d_un.d_ptr);
                if (tmp == 0) {
                    LOGE("Failed to find string table(DT_STRTAB), library: %s", library_name);
                    goto end;
                }
                params.p_dt_strtab = (char *) (base + tmp);
                break;
            case DT_STRSZ:
                params.dt_strtab_size = params.p_pt_dyn->d_un.d_val;
                break;
            case DT_PLTREL:
                params.dt_pltrel = params.p_pt_dyn->d_un.d_val;
                break;
            case DT_JMPREL:
                tmp = FileOffsetToVaddress(params.p_ph_areas, params.p_ehdr->e_phnum,
                                           params.p_pt_dyn->d_un.d_ptr);
                if (tmp == 0) {
                    LOGE("Failed to find jump relocation table (DT_JMPREL), library: %s", library_name);
                }
                params.dt_jmprel.p_dt_jmp_rel = (ElfW(Rel) *) (base + tmp);
                break;
            case DT_PLTRELSZ:
                params.dt_pltrel_size = params.p_pt_dyn->d_un.d_val;
                break;
            case DT_HASH:
                tmp = FileOffsetToVaddress(params.p_ph_areas, params.p_ehdr->e_phnum,
                                           params.p_pt_dyn->d_un.d_ptr);
                if (tmp == 0) {
                    LOGE("Failed to find hash table (DT_HASH), library: %s", library_name);
                    goto end;
                }
                tmp += base;
                params.hash_table.nbucket = *(uint32_t *) tmp;
                params.hash_table.nchain = *(uint32_t *) (tmp + sizeof(uint32_t));
                params.hash_table.bucket = (uint32_t *) (tmp + 2 * sizeof(uint32_t));
                params.hash_table.chain = &params.hash_table.bucket[params.hash_table.nbucket];
                break;
            case DT_GNU_HASH:
                tmp = FileOffsetToVaddress(params.p_ph_areas, params.p_ehdr->e_phnum,
                                           params.p_pt_dyn->d_un.d_ptr);
                if (tmp == 0) {
                    LOGE("Failed to find gnu hash table (DT_GNU_HASH), library: %s", library_name);
                    goto end;
                }
                tmp += base;
                params.gun_hash_table.nbuckets = *(uint32_t *) tmp;
                params.gun_hash_table.symndx = *(uint32_t *) (tmp + sizeof(uint32_t));
                params.gun_hash_table.maskwords = *(uint32_t *) (tmp + sizeof(uint32_t) * 2);
                params.gun_hash_table.shift2 = *(uint32_t *) (tmp + sizeof(uint32_t) * 3);
#if defined(__LP64__)
                params.gun_hash_table.bloom_filter = (uint64_t *) (tmp + sizeof(uint32_t) * 4);
#else
                params.gun_hash_table.bloom_filter = (uint32_t *) (tmp + sizeof(uint32_t) * 4);
#endif
                params.gun_hash_table.bucket = reinterpret_cast<uint32_t *>(&params.gun_hash_table.bloom_filter[params.gun_hash_table.maskwords]);
                params.gun_hash_table.chain = &params.gun_hash_table.bucket[params.gun_hash_table.nbuckets];
                break;
            default:
                break;
        }
    }

    if (params.hash_table.nbucket == 0 && params.gun_hash_table.nbuckets == 0) {
        LOGE("not found DT_HASH or DT_GNU_HASH, unable to find symbol, library: %s", library_name);
        goto end;
    }
    if (params.p_dt_sym == nullptr) {
        LOGE("not found DT_SYMTAB, can't continue, library: %s", library_name);
        goto end;
    }
    if (params.p_dt_strtab == nullptr) {
        LOGE("not found DT_STRTAB, can't continue, library: %s", library_name);
        goto end;
    }
    // 有hash_table就用hash_table,否则用gnu_hash_table

    if (params.hash_table.nbucket != 0) {
        retval = HashLookup(&params, symbol_name, &sym_index);
    } else {
        retval = GnuHashLookup(&params, symbol_name, &sym_index);
    }
    // 不管符号是否导出都会存在符号表中,找不到则说明没有该符号
    if (retval != 0) {
        retval = base + retval;
        goto end;
    }
    if (sym_index == -1 || type == kExported) {
        goto end;
    }

    // 此时再查找JMPREL导入表
    if (params.dt_pltrel == DT_REL) {
        retval = DynamicRelFindSymbol(&params, sym_index);
        retval = retval == 0 ? 0 : base + retval;
    } else if (params.dt_pltrel == DT_RELA) {
        retval = DynamicRelaFindSymbol(&params, sym_index);
        retval = retval == 0 ? 0 : base + retval;
    } else {
        LOGE("unknown pltrel type, only support DT_REL(0x11), DT_RELA(0x7), actual type: 0x%x", params.dt_pltrel);
        goto end;
    }

    end:
    free(params.p_ph_areas);
    return retval;
}

static int ResolveInnerSymbolsAddress(const char *library_name, symbols_address *ret, const char **symbols) {
    int fd;
    ElfW(Ehdr) *head;
    gsize size;
    SymbolParams params = {0};
    gsize section_offset;
    ElfW(Shdr) *sh;
    ElfW(Sym) *sym;

    uint8_t success[ret->len];
    int index = 0;
    int complete = 0;

    while (index < ret->len) {
        success[index++] = 0;
    }
    MapsUtil util;

    char *real_path = util.GetLibraryRealPath(library_name);
    fd = open(real_path, O_RDONLY | O_CLOEXEC);
    free(real_path);
    if (fd < 0) {
        LOGE("open file %s error", library_name);
        return -1;
    }
    size = (gsize) lseek(fd, 0, SEEK_END);

    head = (ElfW(Ehdr) *) mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (head == nullptr) {
        LOGE("mmap file %s failed", library_name);
    } else {
        section_offset = (gsize) ((char *) head + head->e_shoff);    // 节区偏移
        auto *shstr_section = (ElfW(Shdr) *) (section_offset + head->e_shentsize * head->e_shstrndx);    // 节区字符串节区
        char *shstr_table = (char *) head + shstr_section->sh_offset;    // 节区字符串表偏移
        sh = (ElfW(Shdr) *) section_offset;
        for (int i = 0; i < head->e_shnum; ++i, sh++) {
            switch (sh->sh_type) {
                case SHT_STRTAB:
                    // 文件有多个strtab节区,查找名称为.strtab的节区
                    if (strcmp(".strtab", shstr_table + sh->sh_name) == 0) {
                        params.sh_strtab_addr = (gaddress) ((char *) head + sh->sh_offset);
                        params.sh_strtab_offset = sh->sh_offset;
                        params.sh_strtab_size = sh->sh_size;
                    }
                    break;
                case SHT_SYMTAB:
                    params.sh_symtab_addr = (gaddress) ((char *) head + sh->sh_offset);
                    params.sym_num = sh->sh_size / sh->sh_entsize;
                    params.sym_entsize = sh->sh_entsize;
                    params.sh_symtab_offset = sh->sh_offset;
                    break;
                default:
                    break;
            }
        }
        if (params.sh_symtab_addr == 0 || params.sh_strtab_addr == 0) {
            LOGE("not found strtab %" PRIx64 " or symtab %" PRIx64, params.sh_strtab_addr, params.sh_symtab_addr);
            goto mem_error;
        }

        sym = (ElfW(Sym) *) params.sh_symtab_addr;        // 内部符号节区
        for (int i = 0; i < params.sym_num; ++i) {
            const char *value = (const char *) (params.sh_strtab_addr + sym->st_name);
            index = 0;
            while (index < ret->len) {
                if (success[index] == 0 && strcmp(symbols[index], value) == 0) {
                    complete++;
                    success[index] = 1;
                    ret->elements[index] = sym->st_value;
                    break;
                }
                index++;
            }
            if (complete == ret->len) {
                break;
            }
            sym++;
        }

        mem_error:
        {
            munmap(head, size);
        }
    }

    close(fd);
    if (complete != 0) {
        gaddress base = util.GetStartAddress();
        for (int i = 0; i < ret->len; ++i) {
            if (ret->elements[i] != 0) {
                ret->elements[i] += base;
                LOGD("found symbol: %s, library: %s, address: %" PRIx64, symbols[i], library_name, ret->elements[i]);
            }
        }
        return 0;
    }
    return -2;
}

symbols_address *ResolveLibrarySymbolsAddress(const char *library_name, SymbolType type, gsize len, ...) {
    const char *symbols[len];
    va_list ap;
    symbols_address *ret;

    if (len < 1) {
        return nullptr;
    }
    va_start(ap, len);
    for (int i = 0; i < len; ++i) {
        symbols[i] = va_arg(ap, const char*);
    }
    va_end(ap);
    ret = VarLengthObjectAlloc<gaddress>(len);
    if (type != kInner) {
        for (int i = 0; i < ret->len; ++i) {
            ret->elements[i] = ResolveLibrarySymbolAddress(library_name, type, symbols[i]);
        }
        return ret;
    }
    int error = ResolveInnerSymbolsAddress(library_name, ret, symbols);
    if (error == 0) {
        return ret;
    }
    VarLengthObjectFree(ret);
    return nullptr;
}


/*
 * 内部符号不存在内存镜像中,必须走库文件的节区查找
 *
 * */
static gaddress ResolveInnerSymbolAddress(const char *library_name, const char *symbol_name) {
    symbols_address *ret = VarLengthObjectAlloc<gaddress>(1);
    int error = ResolveInnerSymbolsAddress(library_name, ret, &symbol_name);

    gaddress retval = error == 0 ? ret->elements[0] : 0;
    VarLengthObjectFree(ret);
    return retval;
}

static gaddress DynamicRelFindSymbol(ie_find_params *params, int sym_index) {
    ElfW(Rel) *s;

    s = params->dt_jmprel.p_dt_jmp_rel;
    for (int i = 0; i < params->dt_pltrel_size / sizeof(ElfW(Rel)); ++i, s++) {
        if (R_SYM(s->r_info) == sym_index) {
            return s->r_offset;
        }
    }
    return 0;
}

static gaddress DynamicRelaFindSymbol(ie_find_params *params, int sym_index) {
    ElfW(Rela) *s;

    s = params->dt_jmprel.p_dt_jmp_rela;
    for (int i = 0; i < params->dt_pltrel_size / sizeof(ElfW(Rela)); ++i, s++) {
        if (R_SYM(s->r_info) == sym_index) {
            return s->r_offset;
        }
    }
    return 0;
}

static gaddress HashLookup(ie_find_params *params, const char *name, int *sym_index) {
    uint32_t hash = 0, n;
    ElfW(Sym) *s;

    hash = calculate_elf_hash(name);
    for (n = params->hash_table.bucket[hash % params->hash_table.nbucket];
         n != 0; n = params->hash_table.chain[n]) {
        s = params->p_dt_sym + n;
        if (strcmp(params->p_dt_strtab + s->st_name, name) == 0) {
            *sym_index = n;
            return s->st_value;
        }
    }
    *sym_index = -1;
    return 0;
}

static gaddress GnuHashLookup(ie_find_params *params, const char *name, int *sym_index) {
    uint32_t hash, h2, n;
    uint32_t bloom_mask_bits = sizeof(ElfW(Addr)) * 8;
    uint32_t word_num;
    ElfW(Addr) bloom_word;
    ElfW(Sym) *s;

    hash = calculate_gnu_hash(name);
    h2 = hash >> params->gun_hash_table.shift2;

    word_num = (hash / bloom_mask_bits) & params->gun_hash_table.maskwords;
    bloom_word = params->gun_hash_table.bloom_filter[word_num];
    if ((1 & (bloom_word >> (hash % bloom_mask_bits)) & (bloom_word >> (h2 % bloom_mask_bits))) ==
        0) {
        goto find_undefine;
    }

    n = params->gun_hash_table.bucket[hash % params->gun_hash_table.nbuckets];
    if (n == 0) {
        goto find_undefine;
    }
    do {
        s = params->p_dt_sym + n;
        if (((params->gun_hash_table.chain[n] ^ hash) >> 1) == 0 &&
            strcmp(params->p_dt_strtab + s->st_name, name) == 0) {
            *sym_index = n;
            return s->st_value;
        }

    } while ((params->gun_hash_table.chain[n++] & 1) == 0);

    find_undefine:
    for (n = 0, s = params->p_dt_sym; n < params->gun_hash_table.symndx; n++, s++) {
        if (strcmp(params->p_dt_strtab + s->st_name, name) == 0) {
            *sym_index = n;
            return s->st_value;
        }
    }
    *sym_index = -1;
    return 0;
}

static gaddress FileOffsetToVaddress(program_table_area *areas, int num, gsize offset) {
    gaddress retval = 0;
    for (int i = 0; i < num; ++i) {
        if (offset >= areas[i].file_start_offset &&
            offset - areas[i].file_start_offset < areas[i].file_length) {
            retval = offset - areas[i].file_start_offset + areas[i].vaddr_start;
            break;
        }
    }
    return retval;
}