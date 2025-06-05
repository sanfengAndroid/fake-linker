//
// Created by beich on 2020/11/9.
//

#include "linker_common_types.h"

#include <sys/mman.h>

#include "linker_symbol.h"


LinkedListEntry<soinfo> *SoinfoListAllocator::alloc() {
  return fakelinker::linker_symbol.g_soinfo_links_allocator.Get()->alloc();
}

void SoinfoListAllocator::free(LinkedListEntry<soinfo> *entry) {
  fakelinker::linker_symbol.g_soinfo_links_allocator.Get()->free(entry);
}

LinkedListEntry<soinfoT> *SoinfoTListAllocator::alloc() {
  return reinterpret_cast<LinkedListEntry<soinfoT> *>(
    fakelinker::linker_symbol.g_soinfo_links_allocator.Get()->alloc());
}

void SoinfoTListAllocator::free(LinkedListEntry<soinfoT> *entry) {
  fakelinker::linker_symbol.g_soinfo_links_allocator.Get()->free(reinterpret_cast<LinkedListEntry<soinfo> *>(entry));
}

ANDROID_GE_N LinkedListEntry<android_namespace_t> *NamespaceListAllocator::alloc() {
  return fakelinker::linker_symbol.g_namespace_list_allocator.Get()->alloc();
}

ANDROID_GE_N void NamespaceListAllocator::free(LinkedListEntry<android_namespace_t> *entry) {
  fakelinker::linker_symbol.g_namespace_list_allocator.Get()->free(entry);
}

static void LinkerBlockProtectAll(int port) {
  // Since we modify while linker may also be modifying simultaneously, only protect read-write, not read-only
  if ((port & PROT_WRITE) == 0) {
    return;
  }
  fakelinker::linker_symbol.g_soinfo_allocator.Get()->protect_all(port);
  fakelinker::linker_symbol.g_soinfo_links_allocator.Get()->protect_all(port);
  if (android_api >= __ANDROID_API_N__) {
    fakelinker::linker_symbol.g_namespace_allocator.Get()->protect_all(port);
    fakelinker::linker_symbol.g_namespace_list_allocator.Get()->protect_all(port);
  }
}

LinkerBlockLock::LinkerBlockLock() {
  ref_count_++;
  LinkerBlockProtectAll(PROT_READ | PROT_WRITE);
}

LinkerBlockLock::~LinkerBlockLock() {
  if (--ref_count_ == 0) {
    LinkerBlockProtectAll(PROT_READ);
  }
}

size_t LinkerBlockLock::ref_count_ = 0;