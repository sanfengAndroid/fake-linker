#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace fakelinker {
class SymbolResolver {
public:
  struct SymbolResult {
    // Start address of the resolved library/location range
    uintptr_t start;
    // End address of the resolved library/location range
    uintptr_t end;
    // Offset of the current address relative to start
    uintptr_t offset = UINTPTR_MAX;
    // Library name
    const char *name;
  };

  bool AddLibrary(const std::string &name);

  bool AddAddressRange(const std::string &name, uintptr_t start, uintptr_t end);

  /**
   * @brief Format address as follows:
   *         head address [library_name!offset]
   *
   * @param  address        Address to be formatted
   * @param  default_format Whether to format as head address if not in monitoring range
   * @param  head           Add specified header
   * @return std::string
   */
  std::string FormatAddress(uintptr_t address, bool default_format, const std::string &head = "");

  /**
   * @brief Format address at specified buffer location to avoid memory copying
   *
   * @param  buffer         Specify the buffer to be written, which will be updated after writing
   * @param  max_length     Maximum buffer size
   * @param  address        Address to be formatted
   * @param  default_format Whether to format as head address if not in monitoring range
   * @param  head           Add specified header
   * @return true           Within address range
   * @return false          Not within address range
   */
  bool FormatAddressFromBuffer(char *&buffer, size_t max_length, uintptr_t address, bool default_format,
                               const std::string &head = "");

  bool AddAddressRange(const std::string &name, const void *start, const void *end) {
    return AddAddressRange(name, reinterpret_cast<uintptr_t>(start), reinterpret_cast<uintptr_t>(end));
  }

  std::string FormatAddress(const void *address, bool default_format, const std::string &head = "") {
    return FormatAddress(reinterpret_cast<uintptr_t>(address), default_format, head);
  }

  bool FormatAddressFromBuffer(char *&buffer, size_t max_length, const void *address, bool default_format,
                               const std::string &head = "") {
    return FormatAddressFromBuffer(buffer, max_length, reinterpret_cast<uintptr_t>(address), default_format, head);
  }

  bool ResolveSymbol(uintptr_t address, SymbolResult &result);
  bool ResolveSymbol(const void *address, SymbolResult &result) {
    return ResolveSymbol(reinterpret_cast<uintptr_t>(address), result);
  }

private:
  struct LibraryItem {
    uintptr_t start;
    uintptr_t end;
    std::string name;
  };

  LibraryItem *FindLibrary(uintptr_t address);


  std::map<uintptr_t, LibraryItem> libraries_;
};
} // namespace fakelinker