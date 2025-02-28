#pragma once

#include <stdlib.h>

#if defined(__riscv)
// TLS_DTV_OFFSET is a constant used in relocation fields, defined in RISC-V ELF Specification[1]
// The front of the TCB contains a pointer to the DTV, and each pointer in DTV
// points to 0x800 past the start of a TLS block to make full use of the range
// of load/store instructions, refer to [2].
//
// [1]: RISC-V ELF Specification.
// https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-elf.adoc#constants
// [2]: Documentation of TLS data structures
// https://github.com/riscv-non-isa/riscv-elf-psabi-doc/issues/53
#define TLS_DTV_OFFSET 0x800
#else
#define TLS_DTV_OFFSET 0
#endif

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

static constexpr size_t kTlsGenerationNone = 0;
static constexpr size_t kTlsGenerationFirst = 1;

// The first ELF TLS module has ID 1. Zero is reserved for the first word of
// the DTV, a generation count. Unresolved weak symbols also use module ID 0.
static constexpr size_t kTlsUninitializedModuleId = 0;

static inline size_t __tls_module_id_to_idx(size_t id) { return id - 1; }

static inline size_t __tls_module_idx_to_id(size_t idx) { return idx + 1; }

// A descriptor for a single ELF TLS module.
struct TlsModule {
  TlsSegment segment;

  // Offset into the static TLS block or SIZE_MAX for a dynamic module.
  size_t static_offset = SIZE_MAX;

  // The generation in which this module was loaded. Dynamic TLS lookups use
  // this field to detect when a module has been unloaded.
  size_t first_generation = kTlsGenerationNone;

  // Used by the dynamic linker to track the associated soinfo* object.
  void *soinfo_ptr = nullptr;
};
struct soinfo;

void linker_setup_exe_static_tls(const char *progname);
void linker_finalize_static_tls();

void register_soinfo_tls(soinfo *si);
void unregister_soinfo_tls(soinfo *si);

const TlsModule &get_tls_module(size_t module_id);

typedef size_t TlsDescResolverFunc(size_t);

struct TlsIndex {
  size_t module_id;
  size_t offset;
};

struct TlsDescriptor {
#if defined(__arm__)
  size_t arg;
  TlsDescResolverFunc *func;
#else
  TlsDescResolverFunc *func;
  size_t arg;
#endif
};

struct TlsDynamicResolverArg {
  size_t generation;
  TlsIndex index;
};

size_t tlsdesc_resolver_static(size_t);
size_t tlsdesc_resolver_dynamic(size_t);
size_t tlsdesc_resolver_unresolved_weak(size_t);