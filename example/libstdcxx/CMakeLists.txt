cmake_minimum_required(VERSION 3.21)

# This is an example user libstdcxx wrapper target that looks in some special paths for libstdc++.so.6
# You may pick any ordering that works best for your application.
#
# The preference order in this example is:
# 1. The directory of this CMakeLists.txt
# 2. $ENV{QUARTUS_ROOT}/linux64
# 3. The system path

find_package(audit_libstdcxx REQUIRED)

add_library(example_libstdcxx SHARED IMPORTED GLOBAL)

# The libstdcxx target transitive links users to the link_audit_libstdcxx target, which causes all users to get the `-audit` flag
target_link_libraries(example_libstdcxx INTERFACE
  link_audit_libstdcxx
)

# First, we look in the current directory of this CMakeLists.txt for a special low-glibc version libstdstc++.so.6
find_library(
  LIBSTDCXXSO
  "libstdc++.so.6"
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}/
  NO_CACHE
  NO_DEFAULT_PATH
)

# If that is not found, we go grab the next best one from whatever path CMake can find
if( !LIBSTDCXXSO OR "${LIBSTDCXXSO}" STREQUAL "LIBSTDCXXSO-NOTFOUND" )
  find_library(
    LIBSTDCXXSO
    "libstdc++.so.6"
    HINTS $ENV{QUARTUS_ROOT}/linux64
    NO_CACHE
    REQUIRED
  )
endif()

# Resolve any symlinks to proper file paths
file(REAL_PATH ${LIBSTDCXXSO} LIBSTDCXXSO_RESOLVED EXPAND_TILDE)

set_target_properties(example_libstdcxx PROPERTIES
  IMPORTED_LOCATION ${LIBSTDCXXSO_RESOLVED}
  IMPORTED_SONAME "libstdc++.so.6"
)

install(IMPORTED_RUNTIME_ARTIFACTS example_libstdcxx
  LIBRARY DESTINATION lib
)