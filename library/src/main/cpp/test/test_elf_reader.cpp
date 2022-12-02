#include <gtest/gtest.h>

#include "../linker/elf_reader.h"

using namespace fakelinker;

TEST(ElfReader, importTest) {
  ElfReader reader;
  EXPECT_EQ(reader.FindImportSymbol("dlclose"), 0) << "not load library";
  EXPECT_TRUE(reader.LoadFromMemory("libc.so")) << "test load library from memory";

  EXPECT_NE(reader.FindImportSymbol("dlclose"), 0) << "test exist import symbol";
  EXPECT_EQ(reader.FindImportSymbol("no_symbol"), 0) << "test not exist import symbol";

  std::vector<std::string> symbols = {"dlsym", "not_exist0", "dlerror", "not_exist1", ""};
  std::vector<gaddress> addrs = reader.FindImportSymbols(symbols);
  ASSERT_EQ(addrs.size(), symbols.size()) << "find import symbol size";

  EXPECT_NE(addrs[0], 0) << "find import symbol index 0";
  EXPECT_EQ(addrs[1], 0) << "find import symbol index 1";
  EXPECT_NE(addrs[2], 0) << "find import symbol index 2";
  EXPECT_EQ(addrs[3], 0) << "find import symbol index 3";
  EXPECT_EQ(addrs[4], 0) << "find import symbol index 4";
}

TEST(ElfReader, exportTest) {
  ElfReader reader;
  EXPECT_EQ(reader.FindExportSymbol("strncmp"), 0) << "not load library";
  EXPECT_TRUE(reader.LoadFromMemory("libc.so")) << "load library from memory";

  EXPECT_EQ(reader.FindExportSymbol("calloc"), reinterpret_cast<gaddress>(calloc)) << "test find export symbol";
  EXPECT_EQ(reader.FindExportSymbol("not_exist"), 0) << "find not exist export symbol";

  std::vector<std::string> symbols = {"not_exist", "malloc", "not_exist2", "realloc", ""};
  std::vector<gaddress> addrs = reader.FindExportSymbols(symbols);
  ASSERT_EQ(addrs.size(), symbols.size()) << "find export symbol size";

  EXPECT_EQ(addrs[0], 0) << "find export symbol index 0";
  EXPECT_NE(addrs[1], 0) << "find export symbol index 1";
  EXPECT_EQ(addrs[2], 0) << "find export symbol index 2";
  EXPECT_NE(addrs[3], 0) << "find export symbol index 3";
  EXPECT_EQ(addrs[4], 0) << "find export symbol index 4";
}

TEST(ElfReader, internalTest) {
  ElfReader reader;
  EXPECT_EQ(reader.FindInternalSymbol("async_safe_write_log"), 0) << "not load library";
  EXPECT_TRUE(reader.LoadFromDisk("libc.so")) << "load library from memory";

  uint64_t addr = reader.FindInternalSymbol("calloc");

  EXPECT_NE(addr, 0) << "test find export symbol";
  if (addr != 0) {
    EXPECT_EQ(addr, reinterpret_cast<uint64_t>(calloc)) << "verify internal symbol";
  }
  EXPECT_EQ(reader.FindInternalSymbol("not_exist"), 0) << "find not exist export symbol";

  std::vector<std::string> symbols = {"calloc", "not_exist2", "malloc", ""};
  std::vector<gaddress> addrs = reader.FindInternalSymbols(symbols);
  ASSERT_EQ(addrs.size(), symbols.size()) << "find export symbol size";

  EXPECT_NE(addrs[0], 0) << "find internal symbol index 0";
  EXPECT_EQ(addrs[1], 0) << "find internal symbol index 1";
  EXPECT_NE(addrs[2], 0) << "find internal symbol index 2";
  EXPECT_EQ(addrs[3], 0) << "find internal symbol index 3";
}