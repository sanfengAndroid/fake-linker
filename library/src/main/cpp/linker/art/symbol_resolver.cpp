#include <fakelinker/alog.h>
#include <fakelinker/symbol_resolver.h>

#include "../linker_globals.h"

namespace fakelinker {
bool SymbolResolver::AddLibrary(const std::string &name) {
  soinfo *so = ProxyLinker::Get().FindSoinfoByName(name.c_str());
  if (so == nullptr) {
    LOGE("SymbolResolver find library %s failed", name.c_str());
    return false;
  }
  return AddAddressRange(name, static_cast<uintptr_t>(so->base()), static_cast<uintptr_t>(so->base() + so->size()));
}

bool SymbolResolver::AddAddressRange(const std::string &name, uintptr_t start, uintptr_t end) {
  return libraries_
    .try_emplace(start,
                 LibraryItem{
                   .start = start,
                   .end = end,
                   .name = name,
                 })
    .second;
}

std::string SymbolResolver::FormatAddress(uintptr_t address, bool default_format, const std::string &head) {
  char buffer[1024];
  char *ptr = buffer;
  buffer[0] = '\0';
  FormatAddressFromBuffer(ptr, sizeof(buffer), address, default_format, head);
  return buffer;
}

bool SymbolResolver::FormatAddressFromBuffer(char *&buffer, size_t max_length, uintptr_t address, bool default_format,
                                             const std::string &head) {
  if (LibraryItem *item = FindLibrary(address)) {
    if (head.empty()) {
      buffer += snprintf(buffer, max_length, "%p [%s!%p]", reinterpret_cast<void *>(address), item->name.c_str(),
                         reinterpret_cast<void *>(address - item->start));
    } else {
      buffer += snprintf(buffer, max_length, "%s %p [%s!%p]", head.c_str(), reinterpret_cast<void *>(address),
                         item->name.c_str(), reinterpret_cast<void *>(address - item->start));
    }
    return true;
  }
  if (default_format) {
    if (head.empty()) {
      buffer += snprintf(buffer, max_length, "%p", reinterpret_cast<void *>(address));
    } else {
      buffer += snprintf(buffer, max_length, "%s %p", head.c_str(), reinterpret_cast<void *>(address));
    }
  }
  return false;
}

bool SymbolResolver::ResolveSymbol(uintptr_t address, SymbolResolver::SymbolResult &result) {
  if (auto item = FindLibrary(address)) {
    result.start = item->start;
    result.end = item->end;
    result.name = item->name.c_str();
    result.offset = address - item->start;
    return true;
  }
  return false;
}


SymbolResolver::LibraryItem *SymbolResolver::FindLibrary(uintptr_t address) {
  auto itr = libraries_.upper_bound(address);
  if (itr == libraries_.begin()) {
    return nullptr;
  }
  itr--;
  if (itr->second.start <= address && itr->second.end > address) {
    return &itr->second;
  }
  return nullptr;
}

} // namespace fakelinker