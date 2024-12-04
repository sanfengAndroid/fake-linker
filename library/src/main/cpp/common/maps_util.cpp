//
// Created by beich on 2020/11/12.
//

#include <fcntl.h>
#include <sys/stat.h>

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "alog.h"
#include "macros.h"
#include "maps_util.h"

#define MAPS_PATH "/proc/self/maps"

namespace fakelinker {

MapsHelper::MapsHelper(const char *library_name) { GetLibraryProtect(library_name); }

MapsHelper::~MapsHelper() { CloseMaps(); }

bool MapsHelper::GetMemoryProtect(void *address) {
  if (!OpenMaps()) {
    return false;
  }
  auto target = reinterpret_cast<Address>(address);
  while (GetMapsLine()) {
    if (!FormatLine()) {
      continue;
    }
    if (target >= start_address_ && target <= end_address_) {
      PageProtect page;
      page.start = start_address_;
      page.end = end_address_;
      page.file_offset = file_offset_;
      page.old_protect = FormatProtect();
      page.inode = inode_;
      page.path = path_;
      page_.push_back(page);
      return true;
    }
  }
  return false;
}

bool MapsHelper::ReadLibraryMap() {
  bool started = false;
  int32_t found_inode = 0;
  int protect = 0;
  while (GetMapsLine()) {
    if (!started) {
      if (!FormatLine() || !MatchPath() || inode_ == 0 || file_offset_ != 0) {
        continue;
      }
      found_inode = inode_;
      started = true;
    } else if (!FormatLine()) {
      break;
    }
    // 库映射可能是不连续的, 但不会出现交叉
    if (found_inode != inode_ && inode_ != 0) {
      // 严格判断库是否正确, 一个库至少有读写执行内存段
      if ((protect & (kMPRead | kMPWrite | kMPExecute)) != (kMPRead | kMPWrite | kMPExecute)) {
        break;
      } else {
        started = false;
        protect = 0;
        page_.clear();
      }
    }
    if (inode_ == 0) {
      continue;
    }
    PageProtect page;
    page.start = start_address_;
    page.end = end_address_;
    page.old_protect = FormatProtect();
    protect |= page.old_protect;
    page.file_offset = file_offset_;
    page.inode = inode_;
    page.path = path_;
    page_.push_back(page);
  }
  if (page_.empty()) {
    // 文件已读取完
    return true;
  }
  return VerifyLibraryMap();
}

bool MapsHelper::GetLibraryProtect(const char *library_name) {
  if (!library_name || !OpenMaps()) {
    return false;
  }
  MakeLibraryName(library_name);
  do {
    page_.clear();
  } while (!ReadLibraryMap());
  if (!page_.empty()) {
    LOGD("find library: %s\n %s", library_name, ToString().c_str());
  }
  return !page_.empty();
}

bool MapsHelper::UnlockAddressProtect(void *address) {
  if (!GetMemoryProtect(address)) {
    return false;
  }
  PageProtect &page = page_[0];

  if ((page.old_protect & kMPWrite) == kMPWrite) {
    page.new_protect = page.old_protect;
    return true;
  }
  page.new_protect = page.old_protect | kMPRead | kMPWrite;
  // only change rwx protect
  if (mprotect(reinterpret_cast<void *>(page.start), page.end - page.start, page.new_protect & kMPRWX) == 0) {
    return true;
  }
  return false;
}

Address MapsHelper::FindLibraryBase(const char *library_name) {
  Address result = 0;
  if (!library_name || !OpenMaps()) {
    return result;
  }

  int protect = 0;
  MakeLibraryName(library_name);
  while (GetMapsLine()) {
    if (!FormatLine()) {
      continue;
    }
    if (path_[0] == '[') {
      continue;
    }
    if (!MatchPath()) {
      continue;
    }
    if (file_offset_ == 0) {
      result = start_address_;
      protect = 0;
    }
    protect |= FormatProtect();
    if ((protect & (kMPRead | kMPExecute)) != (kMPRead | kMPExecute) && result != 0) {
      break;
    }
  }
  return result;
}

bool MapsHelper::CheckAddressPageProtect(const Address address, uint64_t size, uint8_t prot) {
  if (page_.empty()) {
    return false;
  }
  Address start = address;
  Address end = address + size;
  LOGI("check memory protect 0x%" PRIx64 " - 0x%" PRIx64, start, end);
  for (PageProtect &pp : page_) {
    if (pp.start > end || pp.end < start) {
      continue;
    }
    if ((pp.old_protect & prot) != prot) {
      return false;
    }
    if (pp.end >= end) {
      return true;
    }
    start = pp.end;
  }
  return false;
}

Address MapsHelper::GetLibraryBaseAddress() const {
  if (page_.empty()) {
    return 0;
  }
  uint64_t minoffset = UINT64_MAX;
  Address start = 0;
  for (const PageProtect &pp : page_) {
    if (pp.inode == 0) {
      continue;
    }
    if (pp.file_offset < minoffset) {
      start = pp.start;
      minoffset = pp.file_offset;
    }
  }
  return start;
}

std::string MapsHelper::GetLibraryRealPath(const char *library_name) {
  std::string result;
  if (!library_name || !OpenMaps()) {
    return result;
  }
  MakeLibraryName(library_name);
  while (GetMapsLine()) {
    if (!FormatLine()) {
      continue;
    }
    if (path_[0] == '[' || !MatchPath()) {
      continue;
    }
    result = path_;
    break;
  }
  return result;
}

std::string MapsHelper::GetCurrentRealPath() const {
  for (auto &page : page_) {
    if (!page.path.empty() && page.inode != 0) {
      return page.path;
    }
  }
  return "";
}

std::string MapsHelper::ToString() const {
  std::string result = GetCurrentRealPath() + ":";
  char tmp[16];

  auto IntToHex = [&tmp](Address addr) -> const char * {
    sprintf(tmp, "0x%08" SCNx64, addr);
    return tmp;
  };

  auto ProtToStr = [&tmp](int prot) -> const char * {
    tmp[0] = (prot & kMPRead) == kMPRead ? 'r' : '-';
    tmp[1] = (prot & kMPWrite) == kMPWrite ? 'w' : '-';
    tmp[2] = (prot & kMPExecute) == kMPExecute ? 'x' : '-';
    if (prot & kMPShared) {
      tmp[3] = 's';
    } else if (prot & kMPPrivate) {
      tmp[3] = 'p';
    } else {
      tmp[3] = '-';
    }
    tmp[4] = '\0';
    return tmp;
  };

  for (const PageProtect &pp : page_) {
    result.append("\n  ").append(IntToHex(pp.start)).append(" - ").append(IntToHex(pp.end));
    result.append(" ").append(ProtToStr(pp.old_protect));
    result.append(" ").append(IntToHex(pp.file_offset));
    result.append(" ").append(std::to_string(pp.inode));
  }
  return result;
}

bool MapsHelper::UnlockPageProtect() {
  if (page_.empty()) {
    return false;
  }
  for (PageProtect &pp : page_) {
    if ((pp.old_protect & kMPWrite) != kMPWrite) {
      pp.new_protect = pp.old_protect | kMPWrite | kMPRead;
      if (mprotect(reinterpret_cast<void *>(pp.start), pp.end - pp.start, pp.new_protect & kMPRWX) < 0) {
        return false;
      }
    } else {
      pp.new_protect = pp.old_protect;
    }
  }
  return true;
}

bool MapsHelper::RecoveryPageProtect() {
  if (page_.empty()) {
    return false;
  }
  for (PageProtect &pp : page_) {
    if (pp.old_protect != pp.new_protect) {
      if (mprotect(reinterpret_cast<void *>(pp.start), pp.end - pp.start, pp.old_protect & kMPRWX) < 0) {
        return false;
      }
      pp.new_protect = pp.old_protect;
    }
  }
  return true;
}

bool MapsHelper::GetMapsLine() { return fgets(line_, sizeof(line_), maps_fd_) != nullptr; }

bool MapsHelper::FormatLine() {
  int num = sscanf(line_, "%" SCNx64 "-%" SCNx64 " %s %" SCNx64 "%*s %" SCNi32 " %s", &start_address_, &end_address_,
                   protect_, &file_offset_, &inode_, path_);
  if (num == 5) {
    path_[0] = '\0';
    return true;
  }
  return num == 6;
}

int MapsHelper::FormatProtect() {
  int port = kMPNone;
  for (int i = 0; i < 7 && protect_[i] != '\0'; ++i) {
    switch (protect_[i]) {
    case 'r':
      port |= kMPRead;
      break;
    case 'w':
      port |= kMPWrite;
      break;
    case 'x':
      port |= kMPExecute;
      break;
    case 's':
      port |= kMPShared;
      break;
    case 'p':
      port |= kMPPrivate;
      break;
    }
  }
  return port;
};

bool MapsHelper::MatchPath() {
  char *p = strstr(path_, library_name_.c_str());
  return p != nullptr && strlen(p) == library_name_.length();
}

bool MapsHelper::OpenMaps() {
  if (maps_fd_ != nullptr) {
    fseeko(maps_fd_, 0, SEEK_SET);
    return true;
  }
  maps_fd_ = fopen(MAPS_PATH, "re");
  return maps_fd_ != nullptr;
}

void MapsHelper::CloseMaps() {
  if (maps_fd_ != nullptr) {
    fclose(maps_fd_);
    maps_fd_ = nullptr;
  }
}

bool MapsHelper::MakeLibraryName(const char *library_name) {
  if (__predict_false(!library_name)) {
    return false;
  }
  if (library_name[0] == '/') {
    library_name_ = library_name;
  } else {
    library_name_ = "/";
    library_name_ += library_name;
  }
  return true;
}

/**
 * @brief 验证一个库的有效映射是包含 可执行段
 *
 * @return true
 * @return false
 */
bool MapsHelper::VerifyLibraryMap() {
  for (auto &page : page_) {
    // 模拟器下查找arm库没有可执行权限
    if ((page.old_protect & (kMPExecute | kMPWrite)) != 0) {
      return true;
    }
  }
  return false;
}

} // namespace fakelinker