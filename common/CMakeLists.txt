add_library(audit_libstdcxx_common INTERFACE)
add_library(AuditLibstdcxx::audit_libstdcxx_common ALIAS audit_libstdcxx_common)
target_sources(audit_libstdcxx_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/macros.h)
target_sources(audit_libstdcxx_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/error_types.h)
target_include_directories(audit_libstdcxx_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})