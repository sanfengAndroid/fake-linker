//
// Created by beich on 2020/11/12.
//
#pragma once

#include <sys/mman.h>

#include <string>
#include <vector>

#include "macros.h"

namespace fakelinker {
struct PageProtect {
  Address start = 0;
  Address end;
  uint8_t old_protect;
  uint8_t new_protect;
  uint64_t file_offset;
  int32_t inode;
  std::string path;
};

enum MapsProt {
  kMPNone = PROT_NONE,
  kMPRead = PROT_READ,
  kMPWrite = PROT_WRITE,
  kMPExecute = PROT_EXEC,
  // 与 mman.h 中不同
  kMPShared = 8,
  kMPPrivate = 16,
  kMPRWX = kMPRead | kMPWrite | kMPExecute,
  kMPInvalid = -1,
};

class MapsHelper {
public:
  MapsHelper() = default;

  ~MapsHelper();

  MapsHelper(const char *library_name);

  bool GetMemoryProtect(void *address);

  bool GetLibraryProtect(const char *library_name);

  bool UnlockAddressProtect(void *address);

  Address FindLibraryBase(const char *library_name);

  bool CheckAddressPageProtect(const Address address, uint64_t size, uint8_t prot);

  Address GetLibraryBaseAddress() const;

  std::string GetLibraryRealPath(const char *library_name);

  std::string GetCurrentRealPath() const;

  std::string ToString() const;

  bool UnlockPageProtect();

  bool RecoveryPageProtect();

  operator bool() const { return !page_.empty(); }

private:
  bool GetMapsLine();

  bool FormatLine();

  int FormatProtect();

  bool MatchPath();

  bool OpenMaps();

  void CloseMaps();

  bool ReadLibraryMap();

  bool MakeLibraryName(const char *library_name);

  bool VerifyLibraryMap();

private:
  char line_[PAGE_SIZE + 256 + 1]{};
  char path_[1024]{};
  char protect_[5]{'\0'};
  FILE *maps_fd_ = nullptr;
  std::string library_name_;
  Address start_address_ = 0;
  Address end_address_ = 0;
  uint64_t file_offset_ = 0;
  int32_t inode_ = 0;
  std::vector<PageProtect> page_;
  DISALLOW_COPY_AND_ASSIGN(MapsHelper);
};
} // namespace fakelinker
