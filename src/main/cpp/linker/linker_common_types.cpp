//
// Created by beich on 2020/11/9.
//

#include "linker_common_types.h"

#include <variable_length_object.h>
#include <sys/mman.h>

#include "fake_linker.h"


static LinkerTypeAllocator<soinfo> *g_soinfo_allocator = nullptr;
static LinkerTypeAllocator<LinkedListEntry<soinfo>> *g_soinfo_links_allocator = nullptr;
#if __ANDROID_API__ >= __ANDROID_API_N__
static LinkerTypeAllocator<android_namespace_t> *g_namespace_allocator = nullptr;
static LinkerTypeAllocator<LinkedListEntry<android_namespace_t>> *g_namespace_list_allocator = nullptr;
#endif

static void Init() {
    if (g_soinfo_allocator != nullptr) {
        return;
    }
#ifdef __LP64__
    const char *lib = "/linker64";
#else
    const char *lib = "/linker";
#endif
    VarLengthRef<gaddress> symbols(ResolveLibrarySymbolsAddress(lib, kInner, 4,
                                                                "__dl__ZL18g_soinfo_allocator",
                                                                "__dl__ZL24g_soinfo_links_allocator",
                                                                "__dl__ZL21g_namespace_allocator",
                                                                "__dl__ZL26g_namespace_list_allocator"));
    if (symbols.data == nullptr) {
        LOGE("find symbol g_soinfo_allocator/g_soinfo_links_allocator/g_namespace_allocator/g_namespace_list_allocator failed");
        return;
    }
    g_soinfo_allocator = static_cast<LinkerTypeAllocator<struct soinfo> *>(GSIZE_TO_POINTER(symbols.data->elements[0]));

    g_soinfo_links_allocator = static_cast<LinkerTypeAllocator<LinkedListEntry<struct soinfo>> *>(GSIZE_TO_POINTER(symbols.data->elements[1]));
#if __ANDROID_API__ >= __ANDROID_API_N__
    g_namespace_allocator = static_cast<LinkerTypeAllocator<struct android_namespace_t> *>(GSIZE_TO_POINTER(symbols.data->elements[2]));

    g_namespace_list_allocator = static_cast<LinkerTypeAllocator<LinkedListEntry<struct android_namespace_t>> *>(GSIZE_TO_POINTER(symbols.data->elements[3]));
    CHECK(g_namespace_allocator);
    CHECK(g_namespace_list_allocator);
#endif
    CHECK(g_soinfo_allocator);
    CHECK(g_soinfo_links_allocator);
}

LinkedListEntry<soinfo> *SoinfoListAllocator::alloc() {
    Init();
    return g_soinfo_links_allocator->alloc();
}

void SoinfoListAllocator::free(LinkedListEntry<soinfo> *entry) {
    Init();
    g_soinfo_links_allocator->free(entry);
}

#if __ANDROID_API__ >= __ANDROID_API_N__

LinkedListEntry<android_namespace_t> *NamespaceListAllocator::alloc() {
    Init();
    return g_namespace_list_allocator->alloc();
}

void NamespaceListAllocator::free(LinkedListEntry<android_namespace_t> *entry) {
    Init();
    g_namespace_list_allocator->free(entry);
}

#endif


void linker_block_protect_all(int port) {
    Init();
    // 由于我们修改的同时linker可能也在同时修改，因此只包含读写，不保护读
    if ((port & PROT_WRITE) == 0){
        return;
    }
    g_soinfo_allocator->protect_all(port);
    g_soinfo_links_allocator->protect_all(port);
#if __ANDROID_API__ >= __ANDROID_API_N__
    g_namespace_allocator->protect_all(port);
    g_namespace_list_allocator->protect_all(port);
#endif
}