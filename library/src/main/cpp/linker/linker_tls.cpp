#include "linker_tls.h"

#include <fakelinker/macros.h>

const TlsModule &get_tls_module(size_t module_id) { async_safe_fatal("unsupport get_tls_module"); }

size_t tlsdesc_resolver_static(size_t) { async_safe_fatal("unsupport tlsdesc_resolver_static"); }

size_t tlsdesc_resolver_dynamic(size_t) { async_safe_fatal("unsupport tlsdesc_resolver_dynamic"); }

size_t tlsdesc_resolver_unresolved_weak(size_t) { async_safe_fatal("unsupport tlsdesc_resolver_unresolved_weak"); }