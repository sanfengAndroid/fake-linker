//
// Created by beich on 2020/11/12.
//

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

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
  auto target = reinterpret_cast<gaddress>(address);
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
      page_.push_back(page);
      return true;
    }
  }
  return false;
}

bool MapsHelper::ReadLibraryMap() {
  bool started = false;
  int32_t found_inode = 0;
  while (GetMapsLine()) {
    if (!started) {
      if (!FormatLine() || !MatchPath() || inode_ == 0) {
        continue;
      }
      found_inode = inode_;
      started = true;
    } else if (!FormatLine() || !MatchPath()) {
      break;
    }
    // 不允许库映射段中间包含匿名映射
    if (found_inode != inode_) {
      break;
    }
    PageProtect page;
    real_path_ = path_;
    page.start = start_address_;
    page.end = end_address_;
    page.old_protect = FormatProtect();
    page.file_offset = file_offset_;
    page.inode = inode_;
    page_.push_back(page);
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

/*
 * 查找模块基址忽略内存权限匹配,有些情况并不是r-xp在最前面
 * */
gaddress MapsHelper::FindLibraryBase(const char *library_name) {
  gaddress result = 0;
  if (!library_name || !OpenMaps()) {
    return result;
  }
  MakeLibraryName(library_name);
  while (GetMapsLine()) {
    if (!FormatLine()) {
      continue;
    }
    if (path_[0] == '[') {
      continue;
    }
    if (file_offset_ != 0) {
      continue;
    }
    if (!MatchPath()) {
      continue;
    }
    result = start_address_;
    break;
  }
  return result;
}

bool MapsHelper::CheckAddressPageProtect(const gaddress address, uint64_t size, uint8_t prot) {
  if (page_.empty()) {
    return false;
  }
  gaddress start = address;
  gaddress end = address + size;
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

gaddress MapsHelper::GetLibraryBaseAddress() const {
  if (page_.empty()) {
    return 0;
  }
  uint64_t minoffset = UINT64_MAX;
  gaddress start = 0;
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

gaddress MapsHelper::GetCurrentLineStartAddress() const { return start_address_; }

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

std::string MapsHelper::GetCurrentRealPath() { return real_path_; }

std::string MapsHelper::ToString() const {
  std::string result = real_path_ + ":";
  char tmp[16];

  auto IntToHex = [&tmp](gaddress addr) -> const char * {
    sprintf(tmp, "0x%08" SCNx64, addr);
    return tmp;
  };

  auto ProtToStr = [&tmp](int prot) -> const char * {
    tmp[0] = (prot & kMPRead) == kMPRead ? 'r' : '-';
    tmp[1] = (prot & kMPWrite) == kMPWrite ? 'w' : '-';
    tmp[2] = (prot & kMPEexcute) == kMPEexcute ? 'x' : '-';
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
  return sscanf(line_, "%" SCNx64 "-%" SCNx64 " %s %" SCNx64 "%*s %" SCNi32 " %s", &start_address_, &end_address_,
                protect_, &file_offset_, &inode_, path_) == 6;
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
      port |= kMPEexcute;
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
  return (maps_fd_ != nullptr);
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
  if (page_.empty()) {
    // 说明已经读取完整个maps文件了
    return true;
  }
  for (auto &page : page_) {
    // 模拟器下查找arm库没有可执行权限
    if ((page.old_protect & (kMPEexcute | kMPWrite)) != 0) {
      return true;
    }
  }
  return false;
}

} // namespace fakelinker