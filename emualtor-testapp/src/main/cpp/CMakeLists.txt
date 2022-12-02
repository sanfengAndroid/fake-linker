# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

project(fakelinker-test)
cmake_minimum_required(VERSION 3.4.1)
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
if (${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a" OR ${CMAKE_ANDROID_ARCH_ABI} STREQUAL "x86_64")
    set(MODULE_SUFFIX "64")
else ()
    set(MODULE_SUFFIX "32")
endif ()
add_library(Dobby STATIC IMPORTED)
set_target_properties(Dobby PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/Dobby/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a)

set(FAKELINKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../library/src/main/cpp)

include_directories(${FAKELINKER_DIR}/export)
include_directories(${FAKELINKER_DIR}/common)

add_library( # Sets the name of the library.
        fakelinker-test

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        fakelinker_module.cpp
        ${FAKELINKER_DIR}/common/maps_util.cpp
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
        fakelinker-test

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})

set_target_properties(fakelinker-test PROPERTIES
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

