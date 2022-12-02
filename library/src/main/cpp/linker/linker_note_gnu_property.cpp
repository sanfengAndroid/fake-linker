//
// Created by beich on 2022/5/26.
//

#include "linker_note_gnu_property.h"
#include "linker.h"

#include <sys/auxv.h>

GnuPropertySection::GnuPropertySection(soinfo *si) :
    GnuPropertySection(si->phdr(), si->phnum(), si->load_bias(), si->realpath()) {}

GnuPropertySection::GnuPropertySection(const ElfW(Phdr) * phdr, size_t phdr_count, const ElfW(Addr) load_bias,
                                       const char *name) {
  // Try to find PT_GNU_PROPERTY segment.
  auto note_gnu_property = FindSegment(phdr, phdr_count, load_bias, name);
  // Perform some validity checks.
  if (note_gnu_property && SanityCheck(note_gnu_property, name)) {
    // Parse section.
    Parse(note_gnu_property, name);
  }
}

const ElfW(NhdrGNUProperty) *
  GnuPropertySection::FindSegment(const ElfW(Phdr) * phdr, size_t phdr_count, const ElfW(Addr) load_bias,
                                  const char *name) const {
  // According to Linux gABI extension this segment should contain
  // .note.gnu.property section only.
  if (phdr != nullptr) {
    for (size_t i = 0; i < phdr_count; ++i) {
      if (phdr[i].p_type != PT_GNU_PROPERTY) {
        continue;
      }

      LOGD("\"%s\" PT_GNU_PROPERTY: found at segment index %zu", name, i);

      // Check segment size.
      if (phdr[i].p_memsz < sizeof(ElfW(NhdrGNUProperty))) {
        LOGE("\"%s\" PT_GNU_PROPERTY segment is too small. Segment "
             "size is %zu, minimum is %zu.",
             name, static_cast<size_t>(phdr[i].p_memsz), sizeof(ElfW(NhdrGNUProperty)));
        return nullptr;
      }

      // PT_GNU_PROPERTY contains .note.gnu.property which has SHF_ALLOC
      // attribute, therefore it is loaded.
      auto note_nhdr = reinterpret_cast<ElfW(NhdrGNUProperty) *>(load_bias + phdr[i].p_vaddr);

      // Check that the n_descsz <= p_memsz
      if ((phdr[i].p_memsz - sizeof(ElfW(NhdrGNUProperty))) < note_nhdr->nhdr.n_descsz) {
        LOGE("\"%s\" PT_GNU_PROPERTY segment p_memsz (%zu) is too small for "
             "note n_descsz (%zu).",
             name, static_cast<size_t>(phdr[i].p_memsz), static_cast<size_t>(note_nhdr->nhdr.n_descsz));
        return nullptr;
      }

      return note_nhdr;
    }
  }

  LOGD("\"%s\" PT_GNU_PROPERTY: not found", name);
  return nullptr;
}

bool GnuPropertySection::SanityCheck(const ElfW(NhdrGNUProperty) * note_nhdr, const char *name) const {
  // Check .note section type
  if (note_nhdr->nhdr.n_type != NT_GNU_PROPERTY_TYPE_0) {
    LOGE("\"%s\" .note.gnu.property: unexpected note type. Expected %u, got %u.", name, NT_GNU_PROPERTY_TYPE_0,
         note_nhdr->nhdr.n_type);
    return false;
  }

  if (note_nhdr->nhdr.n_namesz != 4) {
    LOGE("\"%s\" .note.gnu.property: unexpected name size. Expected 4, got %u.", name, note_nhdr->nhdr.n_namesz);
    return false;
  }

  if (strncmp(note_nhdr->n_name, "GNU", 4) != 0) {
    LOGE("\"%s\" .note.gnu.property: unexpected name. Expected 'GNU', got '%s'.", name, note_nhdr->n_name);
    return false;
  }

  return true;
}

bool GnuPropertySection::Parse(const ElfW(NhdrGNUProperty) * note_nhdr, const char *name) {
  // The total length of the program property array is in _bytes_.
  ElfW(Word) offset = 0;
  while (offset < note_nhdr->nhdr.n_descsz) {
    LOGD("\"%s\" .note.gnu.property: processing at offset 0x%x", name, offset);

    // At least the "header" part must fit.
    // The ABI doesn't say that pr_datasz can't be 0.
    if ((note_nhdr->nhdr.n_descsz - offset) < sizeof(ElfW(Prop))) {
      LOGE("\"%s\" .note.gnu.property: no more space left for a "
           "Program Property Note header.",
           name);
      return false;
    }

    // Loop on program property array.
    const ElfW(Prop) *property = reinterpret_cast<const ElfW(Prop) *>(&note_nhdr->n_desc[offset]);
    const ElfW(Word) property_size = align_up(sizeof(ElfW(Prop)) + property->pr_datasz, sizeof(ElfW(Addr)));
    if ((note_nhdr->nhdr.n_descsz - offset) < property_size) {
      LOGE("\"%s\" .note.gnu.property: property descriptor size is "
           "invalid. Expected at least %u bytes, got %u.",
           name, property_size, note_nhdr->nhdr.n_descsz - offset);
      return false;
    }

    // Cache found properties.
    switch (property->pr_type) {
#if defined(__aarch64__)
    case GNU_PROPERTY_AARCH64_FEATURE_1_AND: {
      if (property->pr_datasz != 4) {
        LOGE("\"%s\" .note.gnu.property: property descriptor size is "
             "invalid. Expected %u bytes for "
             "GNU_PROPERTY_AARCH64_FEATURE_1_AND, got %u.",
             name, 4, property->pr_datasz);
        return false;
      }

      const ElfW(Word) flags = *reinterpret_cast<const ElfW(Word) *>(&property->pr_data[0]);
      properties_.bti_compatible = (flags & GNU_PROPERTY_AARCH64_FEATURE_1_BTI) != 0;
      if (properties_.bti_compatible) {
        LOGI("[ BTI compatible: \"%s\" ]", name);
      }
      break;
    }
#endif
    default:
      LOGD("\"%s\" .note.gnu.property: found property pr_type %u pr_datasz 0x%x", name, property->pr_type,
           property->pr_datasz);
      break;
    }

    // Move offset, this should be safe to add because of previous checks.
    offset += property_size;
  }
  return true;
}

#if defined(__aarch64__)
bool GnuPropertySection::IsBTICompatible() const {
  platform_properties g_platform_properties;
  const unsigned long hwcap2 = getauxval(AT_HWCAP2);
  g_platform_properties.bti_supported = (hwcap2 & HWCAP2_BTI) != 0;
  return (g_platform_properties.bti_supported && properties_.bti_compatible);
}
#endif