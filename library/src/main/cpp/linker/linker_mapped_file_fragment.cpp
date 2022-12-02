//
// Created by beich on 2022/5/26.
//

#include "linker_mapped_file_fragment.h"
#include "linker_util.h"

#include <sys/mman.h>

MappedFileFragment::MappedFileFragment() : map_start_(nullptr), map_size_(0), data_(nullptr), size_(0) {}

MappedFileFragment::~MappedFileFragment() {
  if (map_start_ != nullptr) {
    munmap(map_start_, map_size_);
  }
}

bool MappedFileFragment::Map(int fd, off64_t base_offset, size_t elf_offset, size_t size) {
  off64_t offset;
  if (!safe_add(&offset, base_offset, elf_offset)) {
    return false;
  }

  off64_t page_min = page_start(offset);
  off64_t end_offset;

  if (!safe_add(&end_offset, offset, size)) {
    return false;
  }
  if (!safe_add(&end_offset, end_offset, page_offset(offset))) {
    return false;
  }

  size_t map_size = static_cast<size_t>(end_offset - page_min);
  if (!(map_size >= size)) {
    return false;
  }

  uint8_t *map_start = static_cast<uint8_t *>(mmap64(nullptr, map_size, PROT_READ, MAP_PRIVATE, fd, page_min));

  if (map_start == MAP_FAILED) {
    return false;
  }

  map_start_ = map_start;
  map_size_ = map_size;

  data_ = map_start + page_offset(offset);
  size_ = size;

  return true;
}