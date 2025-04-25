cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0144 NEW) # Allow GTEST_ROOT env var to set GTest location

project(AuditLibstdcxx)

option(BUILD_TESTING "Build unit tests with AuditLibstdcxx" ON)

add_subdirectory(common)
add_subdirectory(get_libstdcxx_version)
add_subdirectory(load_libstdcxx)
add_subdirectory(pyaudit)

install(TARGETS link_audit_libstdcxx audit_libstdcxx get_libstdcxx_version EXPORT AuditLibstdcxx
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  FILE_SET HEADERS DESTINATION inc
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
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_compiler_libstdcxx.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/custom_transitive_build_rpath.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/walk_all_targets.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/libstdcxx.cmake
  DESTINATION cmake
)

# Install the python audit script and sitecustomize template
install(IMPORTED_RUNTIME_ARTIFACTS python_audit_libstdcxx python_sitecustomize_template
  RUNTIME DESTINATION pyaudit
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/pyaudit/elftools
  DESTINATION pyaudit
)

if (BUILD_TESTING)
  find_package(GTest)
  if (GTest_FOUND)
    add_subdirectory(test)
  endif()
endif()
