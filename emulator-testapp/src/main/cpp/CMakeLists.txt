project(fakelinker_emulator_test)
cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
  set(MODULE_SUFFIX "64")
else()
  set(MODULE_SUFFIX "32")
endif()

add_library(Dobby STATIC IMPORTED)
set_target_properties(Dobby PROPERTIES IMPORTED_LOCATION
  ${CMAKE_CURRENT_SOURCE_DIR}/Dobby/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a)

set(FAKELINKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../library/src/main/cpp)

add_library(fakelinker-module SHARED
  fakelinker_module.cpp
  ${FAKELINKER_DIR}/common/maps_util.cpp
)

target_include_directories(fakelinker-module PRIVATE
  ${FAKELINKER_DIR}/include
)

add_library(test_module
  SHARED
  test_module.cpp
)

add_library(test_module_hook
  SHARED
  test_module.cpp
)

target_compile_definitions(test_module_hook PUBLIC OPEN_HOOK)

find_library(log-lib log)

target_link_libraries(
  fakelinker-module

  ${log-lib})

set_target_properties(fakelinker-module PROPERTIES
  LINK_FLAGS
  "${LINK_FLAGS} -Wl,--gc-sections,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\""
  SUFFIX "${MODULE_SUFFIX}.so"
)

target_link_libraries(test_module
  ${log-lib}
)

target_link_libraries(test_module_hook
  ${log-lib}
  Dobby
)