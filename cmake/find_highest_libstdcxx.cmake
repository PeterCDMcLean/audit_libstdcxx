# Function: find_highest_libstdcxx
# --------------------------------
# Finds the highest version of `libstdc++.so.6` available in the specified search paths.
#
# Parameters:
#   OPTIMAL_LIBSTDCXX (OUT) - The variable to store the path to the highest version of `libstdc++.so.6` found.
#   SEARCH_PATHS (IN)       - A list of paths to search for `libstdc++.so.6`. Special values:
#                             - "NO_COMPILER_PATH": Excludes the compiler's default library path.
#                             - "NO_DEFAULT_PATH": Excludes the default search paths.
#
# Behavior:
#   - Searches for `libstdc++.so.6` in the provided paths and optionally in the compiler's default path.
#   - Uses a helper target `AuditLibstdcxx::get_libstdcxx_version` to extract the version of the library.
#   - Converts the version from hexadecimal to decimal for comparison.
#   - Updates the `CMAKE_BUILD_RPATH` if the highest version is found in the compiler's default path.
#
# Notes:
#   - If no library is found, the output variable is set to `<OPTIMAL_LIBSTDCXX>-NOTFOUND`.
#   - The function ensures compatibility with GCC compilers by handling their specific library paths.
#
# Example Usage:
#   set(SEARCH_PATHS "/usr/lib" "/usr/local/lib" "NO_COMPILER_PATH")
#   find_highest_libstdcxx(HIGHEST_LIBSTDCXX SEARCH_PATHS)
#   if (HIGHEST_LIBSTDCXX STREQUAL "HIGHEST_LIBSTDCXX-NOTFOUND")
#     message(FATAL_ERROR "No suitable libstdc++.so.6 found!")
#   endif
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/find_compiler_libstdcxx.cmake")

function(find_highest_libstdcxx OPTIMAL_LIBSTDCXX SEARCH_PATHS)
  set(HIGHEST_VALUE 0)
  set(FOUND_LIBRARY FALSE)

  get_target_property(get_libstdcxx_version_path AuditLibstdcxx::get_libstdcxx_version LOCATION)

  list(FIND SEARCH_PATHS "NO_DEFAULT_PATH" NO_DEFAULT_PATH)

  if (NO_DEFAULT_PATH EQUAL -1)
    list(APPEND SEARCH_PATHS "")
  else()
    list(REMOVE_AT SEARCH_PATHS ${NO_DEFAULT_PATH})
    list(REMOVE_ITEM SEARCH_PATHS "")
  endif()

  list(FIND SEARCH_PATHS "NO_COMPILER_PATH" NO_COMPILER_PATH)

  # Find the compiler's libstdc++.so.6 path

  find_compiler_libstdcxx(ABS_LIBSTDCXX_PATH)

  if (NO_COMPILER_PATH EQUAL -1)
    if (ABS_LIBSTDCXX_PATH AND NOT ABS_LIBSTDCXX_PATH STREQUAL "ABS_LIBSTDCXX_PATH-NOTFOUND")
      list(APPEND SEARCH_PATHS "${ABS_LIBSTDCXX_PATH}")
    endif()
  else()
    list(REMOVE_AT SEARCH_PATHS ${NO_COMPILER_PATH})
    if (ABS_LIBSTDCXX_PATH AND NOT ABS_LIBSTDCXX_PATH STREQUAL "ABS_LIBSTDCXX_PATH-NOTFOUND")
      list(REMOVE_ITEM SEARCH_PATHS "${ABS_LIBSTDCXX_PATH}")
    endif()
  endif()

  foreach(PATH IN LISTS SEARCH_PATHS)
    unset(LIB_FOUND)
    if (PATH STREQUAL "")
        find_library(LIB_FOUND "libstdc++.so.6" NO_CACHE)
    else()
        find_library(LIB_FOUND "libstdc++.so.6" PATHS ${PATH} NO_CACHE NO_DEFAULT_PATH)
    endif()

    if (LIB_FOUND AND NOT LIB_FOUND STREQUAL "LIB_FOUND-NOTFOUND")
      execute_process(
        COMMAND ${get_libstdcxx_version_path} ${LIB_FOUND}
        OUTPUT_VARIABLE OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if (OUTPUT MATCHES "^[0-9A-Fa-f]+$")
        string(TOLOWER "${OUTPUT}" OUTPUT)  # Ensure consistent case
        math(EXPR VALUE "0x${OUTPUT}")  # Convert hex to decimal

        if (VALUE GREATER HIGHEST_VALUE)
          set(HIGHEST_VALUE ${VALUE})
          set(${OPTIMAL_LIBSTDCXX} ${LIB_FOUND} PARENT_SCOPE)
          set(FOUND_LIBRARY TRUE)
        endif()
      endif()
    endif()
  endforeach()
  if (NOT FOUND_LIBRARY)
    set(${OPTIMAL_LIBSTDCXX} "${OPTIMAL_LIBSTDCXX}-NOTFOUND" PARENT_SCOPE)
  endif()
endfunction()