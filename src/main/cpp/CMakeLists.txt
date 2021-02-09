# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
string(TOLOWER ${CMAKE_BUILD_TYPE} THIS_BUILD_TYPE)

if (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
	add_definitions(-DUSE_RELA)
else ()
	add_definitions(-D__work_around_b_24465209__)
endif ()

if (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "armeabi-v7a")
	SET(NEON_SRC linker/linker_gnu_hash_neon.cpp)
else ()
	SET(NEON_SRC)
endif ()

include_directories(common)
include_directories(export)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/module_config.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/common/module_config.h")

set(COPY_DIR ${CMAKE_ANDROID_ARCH_ABI})
set(LINKER_LIBRARY_NAME ${LINKER_MODULE_NAME}-${ANDROID_PLATFORM})
if (${MERGE_BUILD} AND ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
	set(COPY_DIR "armeabi-v7a")
	set(LINKER_LIBRARY_NAME ${LINKER_LIBRARY_NAME}64)
elseif (${MERGE_BUILD} AND ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
	set(COPY_DIR "x86")
	set(LINKER_LIBRARY_NAME ${LINKER_LIBRARY_NAME}64)
endif ()

message(STATUS "cmake version:${CMAKE_VERSION} api:${ANDROID_PLATFORM}, build type:${THIS_BUILD_TYPE}, linker module:${LINKER_LIBRARY_NAME}, install module:${HOOK_INSTALL_MODULE_NAME} copy dir:${COPY_DIR}")


add_library( # Sets the name of the library.
		${LINKER_LIBRARY_NAME}

		# Sets the library as a shared library.
		SHARED

		# Provides a relative path to your source file(s).
		linker_main.cpp

		linker/fake_linker.cpp
		linker/linker_globals.cpp
		linker/linker_namespaces.cpp
		linker/linker_soinfo.cpp
		linker/linker_util.cpp
		linker/linker_common_types.cpp
		linker/linker_block_allocator.cpp
		linker/local_block_allocator.cpp
		linker/linker_export.cpp
		${NEON_SRC}

		# JNI Hook
		linker/art/hook_jni_native_interface_impl.cpp
		# JNI Common
		common/jni_helper.cpp
		common/maps_util.cpp
		)

add_executable(
		${HOOK_INSTALL_MODULE_NAME}

		installer/hook_installer.cpp
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
		log-lib

		# Specifies the name of the NDK library that
		# you want CMake to locate.
		log)


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
		${LINKER_LIBRARY_NAME}

		# Links the target library to the log library
		# included in the NDK.
		${log-lib})

set_target_properties(${LINKER_LIBRARY_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/${THIS_BUILD_TYPE}/${COPY_DIR})

set_target_properties(${LINKER_LIBRARY_NAME} PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/${THIS_BUILD_TYPE}/${COPY_DIR})

set_target_properties(${LINKER_LIBRARY_NAME} PROPERTIES
		LINK_FLAGS_RELEASE
		"${LINK_FLAGS_RELEASE} -Wl,--gc-sections,-S,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\"")

target_link_libraries(
		${HOOK_INSTALL_MODULE_NAME}
		${log-lib})

set_target_properties(${HOOK_INSTALL_MODULE_NAME} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY
		${CMAKE_CURRENT_SOURCE_DIR}/../assets/${CMAKE_ANDROID_ARCH_ABI})
