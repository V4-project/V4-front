cmake_minimum_required(VERSION 3.15)
project(
  v4front
  VERSION 0.2.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
option(V4FRONT_BUILD_TESTS "Build tests" ON)
option(V4_FETCH "Fetch V4 headers from Git" OFF)

# ------------------------------------------------------------
# V4 Headers (dependency)
# ------------------------------------------------------------
if(DEFINED V4_INCLUDE_DIR)
  set(_V4_INC_SRC "${V4_INCLUDE_DIR}")
  if(NOT EXISTS "${_V4_INC_SRC}")
    message(FATAL_ERROR "V4_INCLUDE_DIR not found: ${_V4_INC_SRC}")
  endif()
  message(STATUS "Using V4 headers from: ${_V4_INC_SRC}")

elseif(V4_FETCH)
  include(FetchContent)

  set(V4_GIT_REPO
      "https://github.com/kirisaki/v4.git"
      CACHE STRING "V4 Git repository")
  set(V4_GIT_TAG
      "main"
      CACHE STRING "V4 Git tag/branch")

  fetchcontent_declare(
    v4_src
    GIT_REPOSITORY ${V4_GIT_REPO}
    GIT_TAG ${V4_GIT_TAG})
  fetchcontent_makeavailable(v4_src)

  set(_V4_INC_SRC "${v4_src_SOURCE_DIR}/include")
  message(STATUS "Fetched V4 from: ${v4_src_SOURCE_DIR}")

else()
  message(
    FATAL_ERROR
      "V4 headers not specified.\n" "Use one of:\n"
      "  -DV4_INCLUDE_DIR=/path/to/V4/include\n"
      "  -DV4_FETCH=ON -DV4_GIT_REPO=... -DV4_GIT_TAG=...")
endif()

# ------------------------------------------------------------
# V4 Interface Target
# ------------------------------------------------------------
add_library(v4headers INTERFACE)
target_include_directories(v4headers INTERFACE "${_V4_INC_SRC}")

# ------------------------------------------------------------
# Main Library
# ------------------------------------------------------------
add_library(v4front STATIC src/front_compile.cpp)

target_include_directories(v4front PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(v4front PUBLIC v4headers)

# Compile options
if(MSVC)
  target_compile_options(v4front PRIVATE /W4 /GR- /EHs-c- /WX)
else()
  target_compile_options(v4front PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
                                         -fno-rtti)
endif()

# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------
if(V4FRONT_BUILD_TESTS)
  enable_testing()

  add_executable(test_errors tests/test_errors.cpp)
  target_link_libraries(test_errors PRIVATE v4front)

  if(NOT MSVC)
    target_compile_options(test_errors PRIVATE -fno-exceptions -fno-rtti)
  endif()

  add_test(NAME test_errors COMMAND test_errors)
endif()
