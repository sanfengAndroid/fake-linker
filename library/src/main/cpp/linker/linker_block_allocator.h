//
// Created by beich on 2020/11/9.
//
#pragma once

#include <fakelinker/linker_macros.h>
#include <fakelinker/macros.h>


struct LinkerBlockAllocatorPage;

/*
 * https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linker_block_allocator.h
 * */
class LinkerBlockAllocator {
public:
  explicit LinkerBlockAllocator(size_t block_size);

  void *alloc();

  void free(void *block);

  void protect_all(int prot);

  // Purge all pages if all previously allocated blocks have been freed.
  void purge();

private:
  void create_new_page();
  LinkerBlockAllocatorPage *find_page(void *block);

  size_t block_size_;
  LinkerBlockAllocatorPage *page_list_;
  void *free_block_list_;
  ANDROID_GE_Q size_t allocated_;
  DISALLOW_COPY_AND_ASSIGN(LinkerBlockAllocator);
};

template <typename T>
class LinkerTypeAllocator {
public:
  LinkerTypeAllocator() : block_allocator_(sizeof(T)) {}

  T *alloc() { return reinterpret_cast<T *>(block_allocator_.alloc()); }

  void free(T *t) { block_allocator_.free(t); }

  void protect_all(int prot) { block_allocator_.protect_all(prot); }

private:
  LinkerBlockAllocator block_allocator_;
  DISALLOW_COPY_AND_ASSIGN(LinkerTypeAllocator);
};
