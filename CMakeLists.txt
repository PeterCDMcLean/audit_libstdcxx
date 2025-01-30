cmake_minimum_required(VERSION 3.21)

project(audit_libstdcxx)

option(BUILD_TESTING "Build unit tests with audit_libstdcxx" ON)

add_subdirectory(load_libstdcxx)

install(TARGETS link_audit_libstdcxx audit_libstdcxx EXPORT audit_libstdcxx
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib
  FILE_SET HEADERS
)

install(EXPORT audit_libstdcxx DESTINATION cmake)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_libstdcxxConfig.cmake.in"
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/audit_libstdcxxConfig.cmake
  INSTALL_DESTINATION cmake
)

get_target_property(audit_libstdcxx_VERSION audit_libstdcxx VERSION)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/audit_libstdcxxConfigVersion.cmake
  VERSION ${audit_libstdcxx_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/audit_libstdcxxConfig.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/system_libstdcxx.cmake
  DESTINATION cmake
)

if (BUILD_TESTING)
  add_subdirectory(test)
endif()
