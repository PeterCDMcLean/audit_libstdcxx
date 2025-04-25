#-----------------------------
# FindCompilerLibstdcxx.cmake
#-----------------------------
# Provides a function to locate the compiler's libstdc++.so and
# set related directory variables.
# Usage:
#   include(FindCompilerLibstdcxx)
#   find_compiler_libstdcxx(<ABS_DIR_VAR>)
#   # Variables available in the calling scope:
#   #   ABS_DIR_VAR       - Absolute canonical directory path

# Prevent multiple inclusion
include_guard(GLOBAL)

function(find_compiler_libstdcxx abs_dir_var)
  # Only supported for GNU C++
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "find_compiler_libstdcxx: C++ compiler is not GNU, skipping libstdc++ lookup")
    set(${abs_dir_var} "${abs_dir_var}-NOTFOUND" PARENT_SCOPE)
    return()
  endif()

  # Query compiler for its libstdc++.so location
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libstdc++.so
    OUTPUT_VARIABLE LIBSTDCXX_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(LIBSTDCXX_PATH)
    # Extract directory and compute absolute path
    get_filename_component(COMPILER_LIBSTDCXX_DIR "${LIBSTDCXX_PATH}" DIRECTORY)
    file(REAL_PATH "${COMPILER_LIBSTDCXX_DIR}" ABS_LIBSTDCXX_PATH)
    set(${abs_dir_var} "${ABS_LIBSTDCXX_PATH}" PARENT_SCOPE)
  else()
    message(WARNING "find_compiler_libstdcxx: Could not determine the path to GCC libstdc++.so")
    set(${abs_dir_var} "${abs_dir_var}-NOTFOUND" PARENT_SCOPE)
  endif()
endfunction()
