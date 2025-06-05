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
  // Different from mman.h
  kMPShared = 8,
  kMPPrivate = 16,
  kMPReadWrite = kMPRead | kMPWrite,
  kMPRWX = kMPRead | kMPWrite | kMPExecute,
  kMPInvalid = -1,
};

class MapsHelper {
public:
  using iterator = std::vector<PageProtect>::iterator;
  using const_iterator = std::vector<PageProtect>::const_iterator;
  MapsHelper() = default;

  ~MapsHelper();

  MapsHelper(const char *library_name);

  bool GetMemoryProtect(void *address, uint64_t size = 0);

  bool GetLibraryProtect(const char *library_name);

  bool UnlockAddressProtect(void *address, uint64_t size = 0);

  Address FindLibraryBase(const char *library_name);

  bool CheckAddressPageProtect(const Address address, uint64_t size, uint8_t prot);

  Address GetLibraryBaseAddress() const;

  std::string GetLibraryRealPath(const char *library_name);

  std::string GetCurrentRealPath() const;

  std::string ToString() const;

  bool UnlockPageProtect(MapsProt prot = MapsProt::kMPReadWrite);

  bool RecoveryPageProtect();

  operator bool() const { return !page_.empty(); }

  bool empty() const { return page_.empty(); }

  const_iterator begin() const { return page_.begin(); }

  const_iterator end() const { return page_.end(); }

  iterator begin() { return page_.begin(); }

  iterator end() { return page_.end(); }

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
  char line_[4096 + 256 + 1]{};
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
