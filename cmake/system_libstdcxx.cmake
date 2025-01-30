cmake_minimum_required(VERSION 3.21)

if (TARGET system_libstdcxx)
  return()
endif()

add_library(system_libstdcxx SHARED IMPORTED GLOBAL)

# The libstdcxx target transitive links users to the link_audit_libstdcxx target, which causes all users to get the `-audit` flag
target_link_libraries(system_libstdcxx INTERFACE
  link_audit_libstdcxx
)

find_library(
  SYSTEM_LIBSTDCXXSO
  "libstdc++.so.6"
  NO_CACHE
  REQUIRED
)

file(REAL_PATH ${SYSTEM_LIBSTDCXXSO} SYSTEM_LIBSTDCXXSO_RESOLVED EXPAND_TILDE)

set_target_properties(system_libstdcxx PROPERTIES
  IMPORTED_LOCATION ${SYSTEM_LIBSTDCXXSO_RESOLVED}
  IMPORTED_SONAME "libstdc++.so.6"
)
