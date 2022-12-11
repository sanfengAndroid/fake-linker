project(fakelinker_test)
cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

find_package(googletest REQUIRED CONFIG)
find_package(junit-gtest REQUIRED CONFIG)

add_library(fakelinker_test
  SHARED
  test/test_elf_reader.cpp
  test/test_fakelinker.cpp
)

add_executable(fakelinker_static_test
  test/test_elf_reader.cpp
  test/test_fakelinker.cpp
)

set(FAKELINKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../library/src/main/cpp)

option(FAKELINKER_BUILD_STATIC_LIB "build fakelinker static library" ON)
add_subdirectory(${FAKELINKER_DIR} lib)

target_link_libraries(fakelinker_test
  PRIVATE
  fakelinker_static
  googletest::gtest
  junit-gtest::junit-gtest
)

target_compile_definitions(fakelinker_static_test PUBLIC FAKELINKER_STATIC_TEST STD_LOG)

target_link_libraries(fakelinker_static_test PRIVATE googletest::gtest fakelinker_static)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(OUTPUT_TYPE "Debug")
else()
  set(OUTPUT_TYPE "Release")
endif()

set_target_properties(fakelinker_static_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  ${CMAKE_CURRENT_SOURCE_DIR}/build/${OUTPUT_TYPE}/${CMAKE_ANDROID_ARCH_ABI}
)