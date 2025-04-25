cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/find_highest_libstdcxx.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/find_compiler_libstdcxx.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/custom_transitive_build_rpath.cmake")

option(AuditLibstdcxx_LIBSTDCXX_SO_PATHS
  "List of paths to search for libstdc++.so.6 candidate libraries" "")

option(AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_SO
  "Use of `link_libraries` to pseudo-global link `libstdcxx_so` target with CXX,GNU targets" ON)

option(AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_EXE
  "Use of `link_libraries` to pseudo-global link `libstdcxx_exe` target with CXX,GNU executable targets" ON)

# Executable target that links with libstdc++ and applies the audit library
add_library(libstdcxx_exe INTERFACE)
add_library(AuditLibstdcxx::libstdcxx_exe ALIAS libstdcxx_exe)

add_library(AuditLibstdcxx::libstdcxx_so SHARED IMPORTED GLOBAL)

# The libstdcxx target transitive links users to the link_audit_libstdcxx target, which causes all users to get the `-audit` flag
target_link_libraries(libstdcxx_exe INTERFACE
  AuditLibstdcxx::link_audit_libstdcxx
  AuditLibstdcxx::libstdcxx_so
)

find_highest_libstdcxx(LIBSTDCXXSO_PATH "${AuditLibstdcxx_LIBSTDCXX_SO_PATHS}")

file(REAL_PATH ${LIBSTDCXXSO_PATH} LIBSTDCXXSO_RESOLVED_PATH EXPAND_TILDE)

find_compiler_libstdcxx(LIBSTDCXXSO_COMPILER_PATH)
if (LIBSTDCXXSO_COMPILER_PATH AND NOT LIBSTDCXXSO_COMPILER_PATH STREQUAL "LIBSTDCXXSO_COMPILER_PATH-NOTFOUND")
  get_filename_component(LIBSTDCXXSO_DIR "${LIBSTDCXXSO_PATH}" DIRECTORY)
  if ("${LIBSTDCXXSO_DIR}" STREQUAL "${LIBSTDCXXSO_COMPILER_PATH}")
    set_target_properties(AuditLibstdcxx::libstdcxx_so PROPERTIES
      TRANSITIVE_LINK_PROPERTIES "BUILD_RPATH_"
      INTERFACE_BUILD_RPATH_ "${LIBSTDCXXSO_DIR}"
    )
    custom_transitive_build_rpath()
  endif()
endif()

set_target_properties(AuditLibstdcxx::libstdcxx_so PROPERTIES
  IMPORTED_LOCATION ${LIBSTDCXXSO_RESOLVED_PATH}
  IMPORTED_SONAME "libstdc++.so.6"
)

if (AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_SO)

  if(NOT TARGET filter_cxx_gnu )
    add_library(filter_cxx_gnu INTERFACE IMPORTED GLOBAL)
    target_link_libraries(filter_cxx_gnu INTERFACE $<$<LINK_LANG_AND_ID:CXX,GNU>:AuditLibstdcxx::libstdcxx_so>)
  endif()

  add_library(AuditLibstdcxx::filter_cxx_gnu ALIAS filter_cxx_gnu)

  # Pseudo-Global target to filter CXX,GNU and link with libstdcxx_so
  link_libraries(AuditLibstdcxx::filter_cxx_gnu)
endif()

if (AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_EXE)
  if(NOT TARGET filter_cxx_gnu_exe )
    add_library(filter_cxx_gnu_exe INTERFACE IMPORTED GLOBAL)
    target_link_libraries(filter_cxx_gnu_exe INTERFACE $<$<AND:$<LINK_LANG_AND_ID:CXX,GNU>,$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>>:AuditLibstdcxx::libstdcxx_exe>)
  endif()
  add_library(AuditLibstdcxx::filter_cxx_gnu_exe ALIAS filter_cxx_gnu_exe)

  # Pseudo-Global target to filter CXX,GNU executables and link with libstdcxx_exe
  link_libraries(AuditLibstdcxx::filter_cxx_gnu_exe)
endif()

# This customizes the sitecustomize.py file for the build environment.
# Installed environment will differ! As a user, you must configure the sitecustomize template
# for your expected installed environment and path to installed libstdc++.so.6
get_target_property(SITECUSTOMIZE_TEMPLATE_PATH AuditLibstdcxx::python_sitecustomize_template IMPORTED_LOCATION)

set(SITECUSTOMIZE_EXPECTED_LIBSTDCXX_PATH "\"${LIBSTDCXXSO_RESOLVED_PATH}\"")
configure_file(${SITECUSTOMIZE_TEMPLATE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/pyaudit/sitecustomize.py @ONLY)

add_executable(AuditLibstdcxx::python_sitecustomize IMPORTED GLOBAL)
set_target_properties(AuditLibstdcxx::python_sitecustomize PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/pyaudit/sitecustomize.py
)
