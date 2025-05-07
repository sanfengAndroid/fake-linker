#pragma once

#include "macros.h"

class MappedFileFragment {
public:
  MappedFileFragment();
  ~MappedFileFragment();

  bool Map(int fd, off64_t base_offset, size_t elf_offset, size_t size);

  void *data() const { return data_; }
  size_t size() const { return size_; }

private:
  void *map_start_;
  size_t map_size_;
  void *data_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(MappedFileFragment);
};
