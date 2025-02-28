# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.19)

project(fakelinker)

include(GNUInstallDirs)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

option(FAKELINKER_INSTALL_PRIVATE_HEADERS "Install private header files for static linking" OFF)
option(FAKELINKER_BUILD_SHARED_LIBRARY "Enable shared library build" ON)
option(FAKELINKER_INSTALL_TARGET "Enable target installation" ON)

if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -fomit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -fomit-frame-pointer")
endif()

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
  set(NEON_SRC linker/linker_gnu_hash_neon.cpp)
else()
  set(NEON_SRC)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

if(NOT DEFINED FAKELINKER_LOG_LEVEL)
  set(FAKELINKER_LOG_LEVEL 5)
endif()

if(NOT DEFINED FAKELINKER_MODULE_VERSION)
  set(FAKELINKER_MODULE_VERSION 3100)
endif()

if(NOT DEFINED FAKELINKER_MODULE_VERSION_NAME)
  set(FAKELINKER_MODULE_VERSION_NAME "3.1.0")
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/module_config.h.in" "${CMAKE_CURRENT_LIST_DIR}/include/linker_version.h")

message(STATUS "cmake version:${CMAKE_VERSION}
api:${ANDROID_PLATFORM}
build type:${CMAKE_BUILD_TYPE}
install module:${FAKELINKER_INSTALLER_MODULE_NAME}
version: ${FAKELINKER_MODULE_VERSION_NAME}")

set(FAKE_LINKER_SRC
  linker/linker_main.cpp
  linker/linker_globals.cpp
  linker/linker_namespaces.cpp
  linker/linker_relocate.cpp
  linker/linker_soinfo.cpp
  linker/linker_util.cpp
  linker/linker_common_types.cpp
  linker/linker_block_allocator.cpp
  linker/local_block_allocator.cpp
  linker/linker_export.cpp
  linker/elf_reader.cpp
  linker/linker_phdr_16kib_compat.cpp
  linker/linker_mapped_file_fragment.cpp
  linker/linker_note_gnu_property.cpp
  linker/linker_symbol.cpp
  linker/linker_tls.cpp
  ${NEON_SRC}

  # JNI Hook
  linker/art/hook_jni_native_interface_impl.cpp

  # JNI Common
  common/jni_helper.cpp
  common/maps_util.cpp
  common/unique_fd.cpp
  common/unique_memory.cpp

  # xz source
  linker/xz/xz_crc32.c
  linker/xz/xz_crc64.c
  linker/xz/xz_dec_bcj.c
  linker/xz/xz_dec_lzma2.c
  linker/xz/xz_dec_stream.c
)

find_library(log-lib log)

add_library(fakelinker_static
  STATIC
  ${FAKE_LINKER_SRC}
)
target_compile_options(fakelinker_static PRIVATE -fno-rtti -fno-exceptions)
target_include_directories(fakelinker_static PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fakelinker>
)

if(FAKELINKER_INSTALL_PRIVATE_HEADERS)
  target_include_directories(fakelinker_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fakelinker>
  )
endif()

target_link_libraries(fakelinker_static ${log-lib})

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
  target_compile_definitions(fakelinker_static PUBLIC USE_RELA)
else()
  target_compile_definitions(fakelinker_static PUBLIC __work_around_b_24465209__)
endif()

if(FAKELINKER_BUILD_SHARED_LIBRARY)
  add_library(fakelinker SHARED
    linker/linker_dynamic.cpp
  )
  target_link_libraries(fakelinker PRIVATE
    fakelinker_static
    ${log}
  )
  target_include_directories(fakelinker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/fakelinker>
  )

  set_target_properties(fakelinker PROPERTIES
    LINK_FLAGS_RELEASE
    "${LINK_FLAGS} -Wl,--gc-sections,-s,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\""
  )

  if(FAKELINKER_INSTALL_TARGET)
    install(TARGETS fakelinker
      EXPORT fakelinker
    )
  endif()
endif()

if(FAKELINKER_INSTALL_TARGET)
  install(TARGETS fakelinker_static
    EXPORT fakelinker
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fakelinker/fakelinker
  )

  if(FAKELINKER_INSTALL_PRIVATE_HEADERS)
    install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/linker"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fakelinker/fakelinker
      FILES_MATCHING PATTERN "*.h"
    )
  endif()

  install(EXPORT fakelinker
    FILE fakelinker-config.cmake
    NAMESPACE fakelinker::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fakelinker
  )

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/fakelinker-config-version.cmake
    VERSION ${FAKELINKER_MODULE_VERSION_NAME}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/fakelinker-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fakelinker
  )
endif()

if(FAKELINKER_BUILD_INSTALLER)
  add_executable(${FAKELINKER_INSTALLER_MODULE_NAME}
    installer/hook_installer.cpp
  )

  target_link_libraries(${FAKELINKER_INSTALLER_MODULE_NAME}
    ${log-lib})

  set_target_properties(${HOOK_INSTALL_MODULE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/${CMAKE_ANDROID_ARCH_ABI}
  )
endif()
