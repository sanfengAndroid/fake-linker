//
// Created by beich on 2020/11/12.
//

#include "fakelinker/maps_util.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <fakelinker/alog.h>
#include <fakelinker/macros.h>


#define MAPS_PATH "/proc/self/maps"

namespace fakelinker {

static std::string FormatProt(int prot) {
  std::string result(4, '\0');
  result[0] = (prot & kMPRead) == kMPRead ? 'r' : '-';
  result[1] = (prot & kMPWrite) == kMPWrite ? 'w' : '-';
  result[2] = (prot & kMPExecute) == kMPExecute ? 'x' : '-';
  if (prot & kMPShared) {
    result[3] = 's';
  } else if (prot & kMPPrivate) {
    result[3] = 'p';
  } else {
    result[3] = '-';
  }
  return result;
}

static std::string FormatPageProtect(const PageProtect &pp) {
  std::string result;
  char tmp[18];
  auto IntToHex = [&tmp](Address addr) -> const char * {
    sprintf(tmp, "0x%08" SCNx64, addr);
    return tmp;
  };
  result.append(IntToHex(pp.start)).append(" - ").append(IntToHex(pp.end));
  result.append(" ").append(FormatProt(pp.old_protect));
  result.append(" ").append(IntToHex(pp.file_offset));
  result.append(" ").append(std::to_string(pp.inode));
  return result;
}

MapsHelper::MapsHelper(const char *library_name) { GetLibraryProtect(library_name); }

MapsHelper::~MapsHelper() { CloseMaps(); }

bool MapsHelper::GetMemoryProtect(void *address, uint64_t size) {
  if (!OpenMaps()) {
    return false;
  }
  auto target = reinterpret_cast<Address>(address);
  auto end = target + size;
  while (GetMapsLine()) {
    if (!FormatLine()) {
      continue;
    }
    if (end_address_ <= target) {
      continue;
    }
    if (start_address_ >= end) {
      return !pages_.empty();
    }
    PageProtect page;
    page.start = start_address_;
    page.end = end_address_;
    page.file_offset = file_offset_;
    page.old_protect = FormatProtect();
    page.inode = inode_;
    page.path = path_;
    pages_.push_back(page);
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
    // Library mapping may be discontinuous, but there will be no crossing
    if (found_inode != inode_ && inode_ != 0) {
      // Strictly check if library is correct, a library must have at least read-execute memory segment
      if ((protect & (kMPRead | kMPExecute)) == (kMPRead | kMPExecute)) {
        break;
      }
      // continue searching for the next mapping
      started = false;
      protect = 0;
      pages_.clear();
      continue;
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
    pages_.push_back(page);
  }
  if (pages_.empty()) {
    // File has been read completely
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
    pages_.clear();
  } while (!ReadLibraryMap());
  if (!pages_.empty()) {
    LOGD("find library: %s\n %s", library_name, ToString().c_str());
  }
  return !pages_.empty();
}

bool MapsHelper::UnlockAddressProtect(void *address, uint64_t size) {
  if (!GetMemoryProtect(address, size)) {
    return false;
  }
  PageProtect &page = pages_[0];

  if ((page.old_protect & kMPWrite) == kMPWrite) {
    page.new_protect = page.old_protect;
    return true;
  }
  page.new_protect = page.old_protect | kMPRead | kMPWrite;
  // only change rwx protect
  if (int code = mprotect(reinterpret_cast<void *>(page.start), page.end - page.start, page.new_protect & kMPRWX)) {
    LOGE("change protect memory failed: %s, new %s, error: %d", FormatPageProtect(page).c_str(),
         FormatProt(page.new_protect).c_str(), code);
    return false;
  }
  return true;
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
  if (pages_.empty()) {
    return false;
  }
  Address start = address;
  Address end = address + size;
  LOGI("check memory protect 0x%" PRIx64 " - 0x%" PRIx64, start, end);
  for (PageProtect &pp : pages_) {
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
  if (pages_.empty()) {
    return 0;
  }
  uint64_t minoffset = UINT64_MAX;
  Address start = 0;
  for (const PageProtect &pp : pages_) {
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
  for (auto &page : pages_) {
    if (!page.path.empty() && page.inode != 0) {
      return page.path;
    }
  }
  return "";
}

std::string MapsHelper::ToString() const {
  std::string result = GetCurrentRealPath() + ":";
  char tmp[16];


  for (const PageProtect &pp : pages_) {
    result.append("\n  ").append(FormatPageProtect(pp));
  }
  return result;
}

bool MapsHelper::UnlockPageProtect(MapsProt prot) {
  if (pages_.empty() || prot == MapsProt::kMPInvalid) {
    return false;
  }
  for (PageProtect &pp : pages_) {
    if ((pp.old_protect & prot) != prot) {
      pp.new_protect = pp.old_protect | prot;
      if (int code = mprotect(reinterpret_cast<void *>(pp.start), pp.end - pp.start, pp.new_protect & kMPRWX)) {
        LOGE("change protect memory failed: %s, new %s, error: %d", FormatPageProtect(pp).c_str(),
             FormatProt(pp.new_protect).c_str(), code);
        return false;
      }
    } else {
      pp.new_protect = pp.old_protect;
    }
  }
  return true;
}

bool MapsHelper::RecoveryPageProtect() {
  if (pages_.empty()) {
    return false;
  }
  for (PageProtect &pp : pages_) {
    if (pp.old_protect != pp.new_protect) {
      if (int code = mprotect(reinterpret_cast<void *>(pp.start), pp.end - pp.start, pp.old_protect & kMPRWX)) {
        LOGE("change protect memory failed: %s, new %s, error: %d", FormatPageProtect(pp).c_str(),
             FormatProt(pp.new_protect).c_str(), code);
        return false;
      }
      pp.new_protect = pp.old_protect;
    }
  }
  return true;
}

bool MapsHelper::GetMapsLine() { return fgets(line_, sizeof(line_), maps_fd_) != nullptr; }

bool MapsHelper::FormatLine() {
  int num = sscanf(line_, "%" SCNx64 "-%" SCNx64 " %4s %" SCNx64 "%*s %" SCNi32 " %1023s", &start_address_,
                   &end_address_, protect_, &file_offset_, &inode_, path_);
  if (num == 5) {
    path_[0] = '\0';
  }
  // Verify if permission string is valid
  if (FormatProtect() == kMPInvalid) {
    // Since maps file changes frequently, invalid types may be read
    return false;
  }
  return num == 6 || num == 5;
}

int MapsHelper::FormatProtect() {
  int port = kMPNone;
  for (int i = 0; i < sizeof(protect_) && protect_[i] != '\0'; ++i) {
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
    case '-':
      break;
    default:
      return kMPInvalid;
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
 * @brief Verify that a library's valid mapping contains executable segments
 *
 */
bool MapsHelper::VerifyLibraryMap() {
  for (auto &page : pages_) {
    // ARM libraries found under emulator have no executable permissions
    if ((page.old_protect & (kMPExecute | kMPWrite)) != 0) {
      return true;
    }
  }
  pages_.clear();
  return false;
}

} // namespace fakelinker