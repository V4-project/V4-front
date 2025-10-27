cmake_minimum_required(VERSION 3.15)
project(
  v4front
  VERSION 0.2.0
  LANGUAGES C CXX)

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

  # Disable V4's own tests to avoid conflicts
  set(V4_BUILD_TESTS OFF CACHE BOOL "Build V4 tests" FORCE)

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

  # Helper function to add a V4-front test
  # Usage: add_v4front_test(test_name [SOURCE_FILE source.cpp] [LANGUAGE CXX])
  function(add_v4front_test TEST_NAME)
    cmake_parse_arguments(ARG "" "SOURCE_FILE;LANGUAGE" "" ${ARGN})

    # Default source file
    if(NOT ARG_SOURCE_FILE)
      set(ARG_SOURCE_FILE "tests/${TEST_NAME}.cpp")
    endif()

    # Create executable
    add_executable(${TEST_NAME} ${ARG_SOURCE_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE v4front)

    # Set linker language if specified (for C tests)
    if(ARG_LANGUAGE)
      set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE ${ARG_LANGUAGE})
    endif()

    # Platform-specific compile options
    if(MSVC)
      target_compile_definitions(${TEST_NAME} PRIVATE _HAS_EXCEPTIONS=0)
      target_compile_options(${TEST_NAME} PRIVATE /W4 /WX /GR- /EHs- /EHc- /wd4530)
    else()
      target_compile_options(${TEST_NAME} PRIVATE -fno-exceptions -fno-rtti)
    endif()

    # Register test
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  endfunction()

  # Add all C++ tests
  add_v4front_test(test_errors)
  add_v4front_test(test_disasm)
  add_v4front_test(test_compile)
  add_v4front_test(test_arithmetic)
  add_v4front_test(test_comparison)
  add_v4front_test(test_bitwise)
  add_v4front_test(test_stack)
  add_v4front_test(test_case_insensitive)
  add_v4front_test(test_if_then_else)
  add_v4front_test(test_begin_until)
  add_v4front_test(test_while_repeat)
  add_v4front_test(test_begin_again)
  add_v4front_test(test_return_stack_compile)
  add_v4front_test(test_do_loop)
  add_v4front_test(test_word_definitions)
  add_v4front_test(test_memory_access)

  # C test (requires special handling)
  add_v4front_test(test_c SOURCE_FILE tests/test_c.c LANGUAGE CXX)
endif()
