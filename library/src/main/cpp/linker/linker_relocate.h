#pragma once

#include <link.h>

#include <alog.h>

#include "linker_sleb128.h"
#include "linker_soinfo.h"


static constexpr ElfW(Versym) kVersymHiddenBit = 0x8000;

enum RelocationKind { kRelocAbsolute = 0, kRelocRelative, kRelocSymbol, kRelocSymbolCached, kRelocMax };

void count_relocation(RelocationKind kind);

template <bool Enabled>
void count_relocation_if(RelocationKind kind) {
  if (Enabled) {
    count_relocation(kind);
  }
}

inline bool is_symbol_global_and_defined(soinfo *si, const ElfW(Sym) * s) {
  if (__predict_true(ELF_ST_BIND(s->st_info) == STB_GLOBAL || ELF_ST_BIND(s->st_info) == STB_WEAK)) {
    return s->st_shndx != SHN_UNDEF;
  } else if (__predict_false(ELF_ST_BIND(s->st_info) != STB_LOCAL)) {
    LOGW("Warning: unexpected ST_BIND value: %d for \"%s\" in \"%s\" (ignoring)", ELF_ST_BIND(s->st_info),
         si->get_string(s->st_name), si->realpath());
  }
  return false;
}

const size_t RELOCATION_GROUPED_BY_INFO_FLAG = 1;
const size_t RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG = 2;
const size_t RELOCATION_GROUPED_BY_ADDEND_FLAG = 4;
const size_t RELOCATION_GROUP_HAS_ADDEND_FLAG = 8;

template <typename F>
inline bool for_all_packed_relocs(sleb128_decoder decoder, F &&callback) {
  const size_t num_relocs = decoder.pop_front();

  rel_t reloc = {
    .r_offset = decoder.pop_front(),
  };

  for (size_t idx = 0; idx < num_relocs;) {
    const size_t group_size = decoder.pop_front();
    const size_t group_flags = decoder.pop_front();

    size_t group_r_offset_delta = 0;

    if (group_flags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG) {
      group_r_offset_delta = decoder.pop_front();
    }
    if (group_flags & RELOCATION_GROUPED_BY_INFO_FLAG) {
      reloc.r_info = decoder.pop_front();
    }

#if defined(USE_RELA)
    const size_t group_flags_reloc =
      group_flags & (RELOCATION_GROUP_HAS_ADDEND_FLAG | RELOCATION_GROUPED_BY_ADDEND_FLAG);
    if (group_flags_reloc == RELOCATION_GROUP_HAS_ADDEND_FLAG) {
      // Each relocation has an addend. This is the default situation with lld's current encoder.
    } else if (group_flags_reloc == (RELOCATION_GROUP_HAS_ADDEND_FLAG | RELOCATION_GROUPED_BY_ADDEND_FLAG)) {
      reloc.r_addend += decoder.pop_front();
    } else {
      reloc.r_addend = 0;
    }
#else
    if (__predict_false(group_flags & RELOCATION_GROUP_HAS_ADDEND_FLAG)) {
      // This platform does not support rela, and yet we have it encoded in android_rel section.
      async_safe_fatal("unexpected r_addend in android.rel section");
    }
#endif

    for (size_t i = 0; i < group_size; ++i) {
      if (group_flags & RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG) {
        reloc.r_offset += group_r_offset_delta;
      } else {
        reloc.r_offset += decoder.pop_front();
      }
      if ((group_flags & RELOCATION_GROUPED_BY_INFO_FLAG) == 0) {
        reloc.r_info = decoder.pop_front();
      }
#if defined(USE_RELA)
      if (group_flags_reloc == RELOCATION_GROUP_HAS_ADDEND_FLAG) {
        reloc.r_addend += decoder.pop_front();
      }
#endif
      if (!callback(reloc)) {
        return false;
      }
    }

    idx += group_size;
  }

  return true;
}