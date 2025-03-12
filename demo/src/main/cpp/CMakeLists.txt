
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.4.1)
project("demo")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set(FAKELINKER_LOG_LEVEL 0)
endif()

set(FAKELINKER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../library/src/main/cpp)

add_subdirectory(${FAKELINKER_DIR} lib)

find_library(log-lib log)

add_library(static_demo SHARED
  static_demo.cpp
)

target_link_libraries(static_demo
  PRIVATE
  fakelinker_static
  ${log-lib}
)

set_target_properties(static_demo PROPERTIES
  LINK_FLAGS
  "${LINK_FLAGS} -Wl,--gc-sections,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\""
)

add_library(shared_demo SHARED
  shared_demo.cpp
)

target_link_libraries(shared_demo
  PRIVATE
  fakelinker
  ${log-lib}
)

set_target_properties(shared_demo PROPERTIES
  LINK_FLAGS
  "${LINK_FLAGS} -Wl,--gc-sections,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/symbol.map.txt\""
)

add_library(native_test SHARED
  native_test.cpp
)
target_link_libraries(native_test PRIVATE
  ${log-lib}
)