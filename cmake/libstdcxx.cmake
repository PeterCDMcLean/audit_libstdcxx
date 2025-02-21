cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/find_highest_libstdcxx.cmake")

option(AuditLibstdcxx_LIBSTDCXX_SO_PATHS
  "List of paths to search for libstdc++.so.6 candidate libraries" "")

option(AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_SO
  "Use of `link_libraries` to pseudo-global link `libstdcxx_so` target with CXX,GNU targets" ON)

# Executable target that links with libstdc++ and applies the audit library
add_library(libstdcxx_exe INTERFACE)
add_library(AuditLibstdcxx::libstdcxx_exe ALIAS libstdcxx_exe)

add_library(AuditLibstdcxx::libstdcxx_so SHARED IMPORTED GLOBAL)

# The libstdcxx target transitive links users to the link_audit_libstdcxx target, which causes all users to get the `-audit` flag
target_link_libraries(libstdcxx_exe INTERFACE
  AuditLibstdcxx::link_audit_libstdcxx
  AuditLibstdcxx::libstdcxx_so
)

set(LIBSTDCXX_CANDIDATE_PATHS "${CMAKE_CURRENT_SOURCE_DIR}" "${AuditLibstdcxx_LIBSTDCXX_SO_PATHS}")

find_highest_libstdcxx("${LIBSTDCXX_CANDIDATE_PATHS}" LIBSTDCXXSO_PATH)

file(REAL_PATH ${LIBSTDCXXSO_PATH} LIBSTDCXXSO_RESOLVED_PATH EXPAND_TILDE)

set_target_properties(AuditLibstdcxx::libstdcxx_so PROPERTIES
  IMPORTED_LOCATION ${LIBSTDCXXSO_RESOLVED_PATH}
  IMPORTED_SONAME "libstdc++.so.6"
)

if (AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_SO)

  if(NOT TARGET filter_cxx_gnu )
    add_library(filter_cxx_gnu INTERFACE)
    target_link_libraries(filter_cxx_gnu INTERFACE $<$<LINK_LANG_AND_ID:CXX,GNU>:AuditLibstdcxx::libstdcxx_so>)
  endif()

  add_library(AuditLibstdcxx::filter_cxx_gnu ALIAS filter_cxx_gnu)

  # Pseudo-Global target to filter CXX,GNU and link with libstdcxx_so
  link_libraries(AuditLibstdcxx::filter_cxx_gnu)
endif()
