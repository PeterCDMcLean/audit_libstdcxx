cmake_minimum_required(VERSION 3.21)

project(AuditLibstdcxx)

option(BUILD_TESTING "Build unit tests with AuditLibstdcxx" ON)

add_subdirectory(common)
add_subdirectory(get_libstdcxx_version)
add_subdirectory(load_libstdcxx)

install(TARGETS link_audit_libstdcxx audit_libstdcxx get_libstdcxx_version EXPORT AuditLibstdcxx
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  FILE_SET HEADERS
)

install(EXPORT AuditLibstdcxx DESTINATION cmake
NAMESPACE AuditLibstdcxx::)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AuditLibstdcxxConfig.cmake.in"
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/AuditLibstdcxxConfig.cmake
  INSTALL_DESTINATION cmake
)

get_target_property(AuditLibstdcxx_VERSION audit_libstdcxx VERSION)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/AuditLibstdcxxConfigVersion.cmake
  VERSION ${AuditLibstdcxx_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/AuditLibstdcxxConfig.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_highest_libstdcxx.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libstdcxx.cmake
  DESTINATION cmake
)

if (BUILD_TESTING)
  find_package(GTest)
  if (GTest_FOUND)
    add_subdirectory(test)
  endif()
endif()
