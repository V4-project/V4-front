cmake_minimum_required(VERSION 3.20)
project(v4front LANGUAGES C CXX)

# ------------------------------------------------------------
# Global configuration
# ------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------
# Options
#   -DV4_INCLUDE_DIR=... : use a local V4 include path (points to .../include)
#   -DV4_FETCH=ON        : fetch from Git instead of using a local path
# ------------------------------------------------------------
option(V4_FETCH "Fetch V4 headers/defs from Git" OFF)
set(V4_INCLUDE_DIR "" CACHE PATH "Path to V4 include dir (contains v4/...)")
set(V4_GIT_REPO "https://github.com/kirisaki/V4" CACHE STRING "V4 Git repository URL")
set(V4_GIT_TAG "v0.1.2" CACHE STRING "Pinned V4 commit or tag")

# ------------------------------------------------------------
# Resolve V4 include directories
#   - prefer local if given
#   - otherwise fetch from Git
#   - add both source and binary dirs (for generated headers)
# ------------------------------------------------------------
set(_V4_INC_SRC "")
set(_V4_INC_BIN "")

if(V4_INCLUDE_DIR)
  message(STATUS "Using local V4 include: ${V4_INCLUDE_DIR}")
  set(_V4_INC_SRC "${V4_INCLUDE_DIR}")

elseif(V4_FETCH)
  include(FetchContent)
  FetchContent_Declare(v4_src
    GIT_REPOSITORY ${V4_GIT_REPO}
    GIT_TAG        ${V4_GIT_TAG}
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(v4_src)

  set(_V4_INC_SRC "${v4_src_SOURCE_DIR}/include")
  set(_V4_INC_BIN "${v4_src_BINARY_DIR}/include")

  if(NOT EXISTS "${_V4_INC_SRC}")
    message(FATAL_ERROR "V4 include not found at ${_V4_INC_SRC}")
  endif()

  if(NOT (EXISTS "${_V4_INC_SRC}/v4/opcodes.h" OR EXISTS "${_V4_INC_SRC}/v4/opcodes.def"))
    message(WARNING "V4 include directory found, but no v4/opcodes.h or v4/opcodes.def at ${_V4_INC_SRC}/v4")
  endif()

  message(STATUS "Fetched V4 from: ${v4_src_SOURCE_DIR}")

else()
  message(FATAL_ERROR
    "V4 headers not specified.\n"
    "Use one of:\n"
    "  -DV4_INCLUDE_DIR=/path/to/V4/include   (local checkout)\n"
    "  -DV4_FETCH=ON -DV4_GIT_REPO=... -DV4_GIT_TAG=...  (fetch from Git)"
  )
endif()

# ------------------------------------------------------------
# Create an interface target for V4 headers
# ------------------------------------------------------------
add_library(v4headers INTERFACE)
target_include_directories(v4headers INTERFACE
  "${_V4_INC_SRC}"
  "${_V4_INC_BIN}"
)

# ------------------------------------------------------------
# Main library
# ------------------------------------------------------------
add_library(v4front STATIC
  src/front_compile.cpp
)

# Project's own public headers
target_include_directories(v4front PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)

# Link with V4 interface headers
target_link_libraries(v4front PUBLIC v4headers)

# Recommended compile flags
if(MSVC)
  target_compile_options(v4front PRIVATE /W4 /GR- /EHs-c- /WX)
  # Debug: without optimization
  target_compile_options(v4front PRIVATE $<$<CONFIG:Debug>:/Od>)
  # Release: with optimization
  target_compile_options(v4front PRIVATE $<$<CONFIG:Release>:/O1>)
else()
  target_compile_options(v4front PRIVATE
    -Wall -Wextra -Werror
    -fno-exceptions -fno-rtti -Os
  )
endif()

# ------------------------------------------------------------
# Unit tests (vendored doctest)
# ------------------------------------------------------------
enable_testing()

# C++ tests with doctest (original literal tests)
add_executable(v4front-tests
  tests/test_front.cpp
)

# Include vendored doctest and project headers
target_include_directories(v4front-tests PRIVATE
  ${PROJECT_SOURCE_DIR}/tests/vendor/doctest
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(v4front-tests PRIVATE
  v4front
)

if(MSVC)
  target_compile_options(v4front-tests PRIVATE /W4 /GR- /EHs-c- /WX)
  target_compile_options(v4front-tests PRIVATE $<$<CONFIG:Debug>:/Od>)
  target_compile_options(v4front-tests PRIVATE $<$<CONFIG:Release>:/O1>)
else()
  target_compile_options(v4front-tests PRIVATE
    -Wall -Wextra -Werror -Os
    -fno-exceptions -fno-rtti
  )
endif()

target_compile_definitions(v4front-tests PRIVATE
  DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

add_test(NAME v4front-tests COMMAND v4front-tests)

# C smoke test (pure C, original literal tests)
add_executable(v4front-c-tests
  tests/test_c.c
)

# Headers needed by the C test
target_include_directories(v4front-c-tests PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

# Link against the frontend library
target_link_libraries(v4front-c-tests PRIVATE
  v4front
)

# Match flags (no exceptions/RTTI are C++-only, so not needed here)
if(MSVC)
  target_compile_options(v4front-c-tests PRIVATE /W4 /WX)
else()
  target_compile_options(v4front-c-tests PRIVATE
    -Wall -Wextra -Werror -Os
  )
endif()

add_test(NAME v4front-c-tests COMMAND v4front-c-tests)

# Arithmetic operations test (new functionality)
add_executable(v4front-arithmetic-tests
  tests/test_arithmetic.cpp
)

target_include_directories(v4front-arithmetic-tests PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(v4front-arithmetic-tests PRIVATE
  v4front
)

if(MSVC)
  target_compile_options(v4front-arithmetic-tests PRIVATE /W4 /GR- /EHs-c- /WX)
  target_compile_options(v4front-arithmetic-tests PRIVATE $<$<CONFIG:Debug>:/Od>)
  target_compile_options(v4front-arithmetic-tests PRIVATE $<$<CONFIG:Release>:/O1>)
else()
  target_compile_options(v4front-arithmetic-tests PRIVATE
    -Wall -Wextra -Werror -Os
    -fno-exceptions -fno-rtti
  )
endif()

target_compile_definitions(v4front-arithmetic-tests PRIVATE
  DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

add_test(NAME v4front-arithmetic-tests COMMAND v4front-arithmetic-tests)

# ------------------------------------------------------------
# Usage:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release -DV4_INCLUDE_DIR=/path/to/V4/include
#   cmake --build build -j
#   ctest --test-dir build --output-on-failure
# ------------------------------------------------------------