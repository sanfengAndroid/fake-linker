#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace fakelinker {
class SymbolResolver {
public:
  struct SymbolResult {
    // 解析的库/地方范围的起始地址
    uintptr_t start;
    // 解析的库/地方范围的结束地址
    uintptr_t end;
    // 当前地址相对于 start 的偏移
    uintptr_t offset = UINTPTR_MAX;
    // 库名称
    const char *name;
  };

  bool AddLibrary(const std::string &name);

  bool AddAddressRange(const std::string &name, uintptr_t start, uintptr_t end);

  /**
   * @brief 格式化地址如下:
   *         head 地址 [库名!偏移]
   *
   * @param  address        待格式化地址
   * @param  default_format 如果不再监控范围内是否格式化为 head 地址
   * @param  head           添加指定头
   * @return std::string
   */
  std::string FormatAddress(uintptr_t address, bool default_format, const std::string &head = "");

  /**
   * @brief 指定buffer处格式化地址,避免内存拷贝
   *
   * @param  buffer         指定缓冲区
   * @param  max_length     最大缓冲区大小
   * @param  address        待格式化的地址
   * @param  default_format 如果不再监控范围内是否格式化为 head 地址
   * @param  head           添加指定头
   * @return true           在地址范围内
   * @return false          不在地址范围内
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