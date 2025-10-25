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
add_library(v4front STATIC src/compile.cpp src/disasm.cpp)
target_include_directories(v4front PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(v4front PUBLIC v4headers)

# Per-target compile flags (avoid global overrides to reduce MSVC "overriding /EH" noise)
if(MSVC)
  # No exceptions/RTTI + silence C4530 (STL internal try/catch) + keep /WX
  target_compile_definitions(v4front PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(v4front PRIVATE /W4 /WX /GR- /EHs- /EHc- /wd4530)
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

  add_executable(test_disasm tests/test_disasm.cpp)
  target_link_libraries(test_disasm PRIVATE v4front)

  add_executable(test_compile tests/test_compile.cpp)
  target_link_libraries(test_compile PRIVATE v4front)

  add_executable(test_arithmetic tests/test_arithmetic.cpp)
  target_link_libraries(test_arithmetic PRIVATE v4front)

  if(MSVC)
    # Match the library flags so CI on Windows is consistent
    foreach(tgt IN ITEMS test_errors test_disasm)
      target_compile_definitions(${tgt} PRIVATE _HAS_EXCEPTIONS=0)
      target_compile_options(${tgt} PRIVATE /W4 /WX /GR- /EHs- /EHc- /wd4530)
    endforeach()
  else()
    target_compile_options(test_errors PRIVATE -fno-exceptions -fno-rtti)
    target_compile_options(test_disasm PRIVATE -fno-exceptions -fno-rtti)
    target_compile_options(test_compile PRIVATE -fno-exceptions -fno-rtti)
    target_compile_options(test_arithmetic PRIVATE -fno-exceptions -fno-rtti)
  endif()

  add_test(NAME test_errors COMMAND test_errors)
  add_test(NAME test_disasm COMMAND test_disasm)
  add_test(NAME test_compile COMMAND test_compile)
  add_test(NAME test_arithmetic COMMAND test_arithmetic)
endif()
