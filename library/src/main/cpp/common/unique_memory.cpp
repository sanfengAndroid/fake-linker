#include "unique_memory.h"

#include <malloc.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <cstring>

unique_memory::unique_memory(size_t size, bool check) : size_(size) {
  if (size > 0) {
    reset(malloc(size));
    if (check && !ptr_) {
      exit(1);
    }
  }
}

void unique_memory::clean() {
  if (ptr_) {
    memset(ptr_, 0, size_);
  }
}

void unique_memory::reset(void *ptr, size_t size, bool is_mmap) {
  if (ptr_) {
    if (is_mmap_) {
      munmap(ptr_, size_);
    } else {
      free(ptr_);
    }
  }
  if (size != 0) {
    size_ = size;
  }
  ptr_ = ptr;
  is_mmap_ = is_mmap;
}
