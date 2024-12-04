//
// Created by beich on 2020/11/9.
//

#include "linker_block_allocator.h"

#include <sys/mman.h>
#include <sys/prctl.h>

#include <cstdlib>
#include <cstring>

#include <android_level_compat.h>

static constexpr size_t kAllocateSize100 = 4096 * 100;
static constexpr size_t kAllocateSize = 4096;
static constexpr size_t kAllocateSize96 = 65536 * 6;

struct LinkerBlockAllocatorPage {
  LinkerBlockAllocatorPage *next;
  uint8_t bytes[kAllocateSize100 - 16] __attribute__((aligned(16)));
};

struct FreeBlockInfo {
  void *next_block;
  size_t num_free_blocks;
};

static size_t get_block_alloca_size() {
  if (android_api >= __ANDROID_API_U__) {
    return kAllocateSize96;
  }
  if (android_api >= __ANDROID_API_Q__) {
    return kAllocateSize100;
  }
  return kAllocateSize;
}

static size_t sizeof_block_alloca_page() {
  return android_api >= __ANDROID_API_Q__ ? sizeof(LinkerBlockAllocatorPage) : kAllocateSize;
}

// the align should be power of 2
static constexpr size_t round_up(size_t size, size_t align) { return (size + (align - 1)) & ~(align - 1); }

static constexpr size_t kBlockSizeMin = sizeof(FreeBlockInfo);
static constexpr size_t kBlockSizeAlign = sizeof(void *);

LinkerBlockAllocator::LinkerBlockAllocator(size_t block_size) :
    block_size_(round_up(block_size < kBlockSizeMin ? kBlockSizeMin : block_size, 16)), page_list_(nullptr),
    free_block_list_(nullptr) {
  if (android_api >= __ANDROID_API_Q__) {
    allocated_ = 0;
  }
}

void *LinkerBlockAllocator::alloc() {
  protect_all(PROT_READ | PROT_WRITE);
  if (free_block_list_ == nullptr) {
    create_new_page();
  }

  auto *block_info = reinterpret_cast<FreeBlockInfo *>(free_block_list_);
  if (block_info->num_free_blocks > 1) {
    auto *next_block_info = reinterpret_cast<FreeBlockInfo *>(reinterpret_cast<char *>(free_block_list_) + block_size_);
    next_block_info->next_block = block_info->next_block;
    next_block_info->num_free_blocks = block_info->num_free_blocks - 1;
    free_block_list_ = next_block_info;
  } else {
    free_block_list_ = block_info->next_block;
  }

  memset(block_info, 0, block_size_);
  if (android_api >= __ANDROID_API_Q__) {
    ++allocated_;
  }
  return block_info;
}

void LinkerBlockAllocator::free(void *block) {
  if (block == nullptr) {
    return;
  }
  protect_all(PROT_READ | PROT_WRITE);
  LinkerBlockAllocatorPage *page = find_page(block);
  ssize_t offset = reinterpret_cast<uint8_t *>(block) - page->bytes;
  if (offset % block_size_ != 0) {
    LOGW("The current page has a memory block that cannot be released: %p, "
         "offset: %zx, May cause memory leaks.",
         page, offset);
    //		abort();
    return;
  }

  memset(block, 0, block_size_);

  auto *block_info = reinterpret_cast<FreeBlockInfo *>(block);

  block_info->next_block = free_block_list_;
  block_info->num_free_blocks = 1;

  free_block_list_ = block_info;
  if (android_api >= __ANDROID_API_Q__) {
    --allocated_;
  }
}

void LinkerBlockAllocator::protect_all(int prot) {
  for (LinkerBlockAllocatorPage *page = page_list_; page != nullptr; page = page->next) {
    if (mprotect(page, get_block_alloca_size(), prot) == -1) {
      abort();
    }
  }
}

void LinkerBlockAllocator::create_new_page() {
  auto *page = reinterpret_cast<LinkerBlockAllocatorPage *>(
    mmap(nullptr, get_block_alloca_size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  if (page == MAP_FAILED) {
    abort(); // oom
  }
  prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, page, get_block_alloca_size(), "linker_alloc");
  auto *first_block = reinterpret_cast<FreeBlockInfo *>(page->bytes);
  first_block->next_block = free_block_list_;
  first_block->num_free_blocks = (get_block_alloca_size() - 16) / block_size_;
  free_block_list_ = first_block;
  page->next = page_list_;
  page_list_ = page;
}

LinkerBlockAllocatorPage *LinkerBlockAllocator::find_page(void *block) {
  if (block == nullptr) {
    abort();
  }
  LinkerBlockAllocatorPage *page = page_list_;
  while (page != nullptr) {
    const auto *page_ptr = reinterpret_cast<const uint8_t *>(page);
    if (block >= (page_ptr + sizeof(page->next)) && block < (page_ptr + get_block_alloca_size())) {
      return page;
    }
    page = page->next;
  }
  abort();
}

void LinkerBlockAllocator::purge() {
  if (android_api >= __ANDROID_API_Q__ && allocated_) {
    return;
  }
  LinkerBlockAllocatorPage *page = page_list_;
  while (page) {
    LinkerBlockAllocatorPage *next = page->next;
    munmap(page, get_block_alloca_size());
    page = next;
  }
  page_list_ = nullptr;
  free_block_list_ = nullptr;
}
