#pragma once

#include <link.h>

struct address_space_params {
  void *start_addr = nullptr;
  size_t reserved_size = 0;
  bool must_use_address = false;
};

struct platform_properties {
#if defined(__aarch64__)
  bool bti_supported = false;
#endif
};