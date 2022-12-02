//
// Created by beich on 2020/11/13.
//

#include "local_block_allocator.h"
#include <malloc.h>
#include <string.h>

void *LocalBlockAllocator::alloc() const { return malloc(block_size_); }

void LocalBlockAllocator::free(void *block) {
  if (block != nullptr) {
    free(block);
  }
}

void LocalBlockAllocator::purge() {}