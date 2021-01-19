//
// Created by beich on 2020/11/9.
//
#pragma once

#include <macros.h>

struct LinkerBlockAllocatorPage;

/*
 * This class is a non-template version of the LinkerTypeAllocator
 * It keeps code inside .cpp file by keeping the interface
 * template-free.
 *
 * Please use LinkerTypeAllocator<type> where possible (everywhere).
 */
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
	size_t allocated_;

	DISALLOW_COPY_AND_ASSIGN(LinkerBlockAllocator);
};

/*
 * A simple allocator for the dynamic linker. An allocator allocates instances
 * of a single fixed-size type. Allocations are backed by page-sized private
 * anonymous mmaps.
 *
 * The differences between this allocator and BionicAllocator are:
 * 1. This allocator manages space more efficiently. BionicAllocator operates in
 *    power-of-two sized blocks up to 1k, when this implementation splits the
 *    page to aligned size of structure; For example for structures with size
 *    513 this allocator will use 516 (520 for lp64) bytes of data where
 *    generalized implementation is going to use 1024 sized blocks.
 *
 * 2. This allocator does not munmap allocated memory, where BionicAllocator does.
 *
 * 3. This allocator provides mprotect services to the user, where BionicAllocator
 *    always treats its memory as READ|WRITE.
 */
template<typename T>
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
