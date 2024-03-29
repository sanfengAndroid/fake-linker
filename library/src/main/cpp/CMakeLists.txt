# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

project(fakelinker)
cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
  add_compile_definitions(USE_RELA)
  set(MODULE_SUFFIX "64")
else()
  add_compile_definitions(__work_around_b_24465209__)
  set(MODULE_SUFFIX "32")
endif()

if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
  set(NEON_SRC linker/linker_gnu_hash_neon.cpp)
else()
  set(NEON_SRC)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/module_config.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/export/linker_version.h")

message(STATUS "cmake version:${CMAKE_VERSION}
api:${ANDROID_PLATFORM}
build type:${CMAKE_BUILD_TYPE}
linker module:${LINKER_MODULE_NAME}
install module:${INSTALLER_MODULE_NAME}
version: ${MODULE_VERSION_NAME}")

set(FAKE_LINKER_SRC
  linker/linker_main.cpp
  linker/linker_globals.cpp
  linker/linker_namespaces.cpp
  linker/linker_soinfo.cpp
  linker/linker_util.cpp
  linker/linker_common_types.cpp
  linker/linker_block_allocator.cpp
  linker/local_block_allocator.cpp
  linker/linker_export.cpp
  linker/elf_reader.cpp
  linker/linker_mapped_file_fragment.cpp
  linker/linker_note_gnu_property.cpp
  linker/linker_symbol.cpp
  ${NEON_SRC}

  # JNI Hook
  linker/art/hook_jni_native_interface_impl.cpp

  # JNI Common
  common/jni_helper.cpp
  common/maps_util.cpp
  common/unique_fd.cpp
  common/unique_memory.cpp
)

find_library(log-lib log)

if(FAKELINKER_BUILD_STATIC_LIB)
  add_library(fakelinker_static
    STATIC
    ${FAKE_LINKER_SRC}
  )
  target_compile_options(fakelinker_static PRIVATE -fno-rtti -fno-exceptions)
  target_include_directories(fakelinker_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common ${CMAKE_CURRENT_SOURCE_DIR}/export)
  target_link_libraries(fakelinker_static ${log-lib})
else()
  add_library(fakelinker
    SHARED
    ${FAKE_LINKER_SRC}
  )

  target_compile_options(fakelinker PRIVATE -fno-rtti -fno-exceptions)
  target_link_libraries(fakelinker ${log-lib})
  target_include_directories(fakelinker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common ${CMAKE_CURRENT_SOURCE_DIR}/export)
  set_target_properties(fakelinker PROPERTIES
    LINK_FLAGS_RELEASE
    "${LINK_FLAGS} -Wl,--gc-sections,-s,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\""
    SUFFIX "${MODULE_SUFFIX}.so"
    OUTPUT_NAME ${LINKER_MODULE_NAME}
  )
endif()

if(FAKELINKER_BUILD_INSTALLER)
  add_executable(${INSTALLER_MODULE_NAME}
    installer/hook_installer.cpp
  )

  target_link_libraries(${INSTALLER_MODULE_NAME}
    ${log-lib})

  set_target_properties(${HOOK_INSTALL_MODULE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/${CMAKE_ANDROID_ARCH_ABI})
endif()
