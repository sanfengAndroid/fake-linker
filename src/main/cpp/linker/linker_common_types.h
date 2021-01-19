/*
 * Copyright (C) 2016 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "linked_list.h"
#include "linker_block_allocator.h"


struct soinfo;
struct android_namespace_t;

class SoinfoListAllocator {
public:
	static LinkedListEntry<soinfo> *alloc();

	static void free(LinkedListEntry<soinfo> *entry);

private:
	// unconstructable
	DISALLOW_IMPLICIT_CONSTRUCTORS(SoinfoListAllocator);
};

class NamespaceListAllocator {
public:
	static LinkedListEntry<android_namespace_t> *alloc();

	static void free(LinkedListEntry<android_namespace_t> *entry);

private:
	// unconstructable
	DISALLOW_IMPLICIT_CONSTRUCTORS(NamespaceListAllocator);
};

template<size_t size>
class SizeBasedAllocator {
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
	static LinkerBlockAllocator allocator_;
};



template<typename T>
class TypeBasedAllocator {
public:
	static T *alloc() {
		return reinterpret_cast<T *>(SizeBasedAllocator<sizeof(T)>::alloc());
	}

	static void free(T *ptr) {
		SizeBasedAllocator<sizeof(T)>::free(ptr);
	}

	static void purge() {
		SizeBasedAllocator<sizeof(T)>::purge();
	}
};

template<size_t size>
LinkerBlockAllocator SizeBasedAllocator<size>::allocator_(size);

typedef LinkedList<soinfo, SoinfoListAllocator> soinfo_list_t;
typedef LinkedList<android_namespace_t, NamespaceListAllocator> android_namespace_list_t;

template<typename T>
using linked_list_t = LinkedList<T, TypeBasedAllocator<LinkedListEntry<T>>>;

typedef linked_list_t<soinfo> SoinfoLinkedList;

void linker_block_protect_all(int port);