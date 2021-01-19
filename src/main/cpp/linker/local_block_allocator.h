//
// Created by beich on 2020/11/13.
//
#pragma once

#include <stdint.h>
#include <macros.h>
#include "linked_list.h"

struct symbol_relocation;

class LocalBlockAllocator {
public:
	explicit LocalBlockAllocator(size_t block_size) : block_size_(block_size) {}

	void *alloc() const;

	void free(void *block);

	void purge();

private:
	size_t block_size_;
	DISALLOW_COPY_AND_ASSIGN(LocalBlockAllocator);
};

template<size_t size>
class LocalSizeBasedAllocator {
public:
	static void *alloc() {
		return allocator_.alloc();
	}

	static void free(void *ptr) {
		allocator_.free(ptr);
	}

	static void purge() {
		allocator_.purge();
	}

private:
	static LocalBlockAllocator allocator_;
};

template<typename T>
class LocalTypeBasedAllocator {
public:
	static T *alloc() {
		return reinterpret_cast<T *>(LocalSizeBasedAllocator<sizeof(T)>::alloc());
	}

	static void free(T *ptr) {
		LocalSizeBasedAllocator<sizeof(T)>::free(ptr);
	}

	static void purge() {
		LocalSizeBasedAllocator<sizeof(T)>::purge();
	}
};

template<size_t size>
LocalBlockAllocator LocalSizeBasedAllocator<size>::allocator_(size);

template<typename T>
using local_linked_list_t = LinkedList<T, LocalTypeBasedAllocator<LinkedListEntry<T>>>;

typedef local_linked_list_t<symbol_relocation> SymbolRelocationLinkedList;
