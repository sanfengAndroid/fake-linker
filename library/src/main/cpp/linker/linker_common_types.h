#pragma once

#include <sys/mman.h>

#include <linker_macros.h>

#include "linked_list.h"
#include "linker_block_allocator.h"

struct soinfo;
struct soinfoT;
struct android_namespace_t;

class SoinfoListAllocator {
public:
  static LinkedListEntry<soinfo> *alloc();

  static void free(LinkedListEntry<soinfo> *entry);

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SoinfoListAllocator);
};

class SoinfoTListAllocator {
public:
  static LinkedListEntry<soinfoT> *alloc();

  static void free(LinkedListEntry<soinfoT> *entry);

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SoinfoTListAllocator);
};


ANDROID_GE_N class NamespaceListAllocator {
public:
  ANDROID_GE_N static LinkedListEntry<android_namespace_t> *alloc();

  ANDROID_GE_N static void free(LinkedListEntry<android_namespace_t> *entry);

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NamespaceListAllocator);
};

template <size_t size>
class SizeBasedAllocator {
public:
  static void *alloc() { return allocator_.alloc(); }

  static void free(void *ptr) { allocator_.free(ptr); }

  static void purge() { allocator_.purge(); }

private:
  static LinkerBlockAllocator allocator_;
};

template <typename T>
class TypeBasedAllocator {
public:
  static T *alloc() { return reinterpret_cast<T *>(SizeBasedAllocator<sizeof(T)>::alloc()); }

  static void free(T *ptr) { SizeBasedAllocator<sizeof(T)>::free(ptr); }

  static void purge() { SizeBasedAllocator<sizeof(T)>::purge(); }
};

template <size_t size>
LinkerBlockAllocator SizeBasedAllocator<size>::allocator_(size);

typedef LinkedList<soinfo, SoinfoListAllocator> soinfo_list_t;
typedef LinkedListT<soinfo, SoinfoListAllocator> soinfo_list_t_T;
typedef LinkedListWrapper<soinfo, SoinfoListAllocator> soinfo_list_t_wrapper;

typedef LinkedList<android_namespace_t, NamespaceListAllocator> android_namespace_list_t;
typedef LinkedListT<android_namespace_t, NamespaceListAllocator> android_namespace_list_t_T;
typedef LinkedListWrapper<android_namespace_t, NamespaceListAllocator> android_namespace_list_t_wrapper;

template <typename T>
using linked_list_t = LinkedList<T, TypeBasedAllocator<LinkedListEntry<T>>>;

template <typename T>
using linked_list_t_T = LinkedListT<T, TypeBasedAllocator<LinkedListEntry<T>>>;

template <typename T>
using linked_list_t_wrapper = LinkedListWrapper<T, TypeBasedAllocator<LinkedListEntry<T>>>;

typedef linked_list_t<soinfo> SoinfoLinkedList;
typedef linked_list_t_T<soinfo> SoinfoLinkedListT;
typedef linked_list_t_wrapper<soinfo> SoinfoLinkedListWrapper;

class LinkerBlockLock {

public:
  LinkerBlockLock();

  ~LinkerBlockLock();

private:
  static size_t ref_count_;
};
