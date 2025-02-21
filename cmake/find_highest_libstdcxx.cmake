
function(find_highest_libstdcxx SEARCH_PATHS OPTIMAL_LIBSTDCXX)
  set(HIGHEST_VALUE 0)
  set(FOUND_LIBRARY FALSE)

  get_target_property(get_libstdcxx_version_path AuditLibstdcxx::get_libstdcxx_version LOCATION)

  list(APPEND SEARCH_PATHS "")
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