//
// Created by beich on 2020/11/13.
//

#include <malloc.h>
#include "local_block_allocator.h"


void *LocalBlockAllocator::alloc() const {
	return malloc(block_size_);
}


void LocalBlockAllocator::free(void *block) {
	if (block != nullptr) {
		free(block);
	}
}

void LocalBlockAllocator::purge() {

}
