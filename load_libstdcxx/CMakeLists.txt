# Checks the default system libstdc++.so version and loads that if it is compatible
cmake_minimum_required(VERSION 3.23)

# We can't link directly against the audit library
# So, we use an interface library that has an INTERFACE_LINK_LIBRARY with the
# --audit flag
add_library(link_audit_libstdcxx INTERFACE)
set_target_properties(link_audit_libstdcxx PROPERTIES VERSION 1.0.0)

include(GenerateExportHeader)

# Do not link directly aginst this shared library. It should be referenced via, preferably, DT_AUDIT
add_library(audit_libstdcxx SHARED)
set_target_properties(audit_libstdcxx PROPERTIES C_VISIBILITY_PRESET hidden)

generate_export_header(audit_libstdcxx)
target_include_directories(audit_libstdcxx PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Installing the export headers is only necessary if we want to manually import the library
# to test the functionality outside of DT_AUDIT
#target_sources(audit_libstdcxx PUBLIC FILE_SET HEADERS TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR} FILES
#  ${CMAKE_CURRENT_BINARY_DIR}/audit_libstdcxx_export.h
#)

target_sources(audit_libstdcxx PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/audit.c
)

target_link_libraries(audit_libstdcxx PRIVATE c dl)

target_link_options(audit_libstdcxx PRIVATE -Wl,-zdefs)

set_target_properties(audit_libstdcxx PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_target_properties(audit_libstdcxx PROPERTIES VERSION 1.0.0)

target_compile_definitions(audit_libstdcxx PRIVATE AUDIT_LIBSTDCXX_FILE_NAME="$<TARGET_FILE_NAME:audit_libstdcxx>")

# `target_link_options` cannot be used as $ORIGIN escaping is not functional
# target_link_libraries appears to behave MUCH better when it passes $ORIGIN to the build system
# ALSO, it appears that support for $ORIGIN escaping in Ninja differs from Makefiles. Probably contributes to the above problem

if ( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
  target_link_libraries(link_audit_libstdcxx INTERFACE
    $<$<BOOL:$<TARGET_PROPERTY:AUDIT_LIBRARIES>>:-Wl$<COMMA>--audit$<COMMA>'$<TARGET_PROPERTY:AUDIT_LIBRARIES>'>
    $<$<NOT:$<BOOL:$<TARGET_PROPERTY:AUDIT_LIBRARIES>>>:-Wl$<COMMA>--audit$<COMMA>'\$$ORIGIN/$<PATH:RELATIVE_PATH,$<TARGET_FILE:audit_libstdcxx>,$<TARGET_PROPERTY:BINARY_DIR>>'>
  )
else()
  target_link_libraries(link_audit_libstdcxx INTERFACE
    $<$<BOOL:$<TARGET_PROPERTY:AUDIT_LIBRARIES>>:-Wl$<COMMA>--audit$<COMMA>'$<TARGET_PROPERTY:AUDIT_LIBRARIES>'>
    $<$<NOT:$<BOOL:$<TARGET_PROPERTY:AUDIT_LIBRARIES>>>:-Wl$<COMMA>--audit$<COMMA>'\$ORIGIN/$<PATH:RELATIVE_PATH,$<TARGET_FILE:audit_libstdcxx>,$<TARGET_PROPERTY:BINARY_DIR>>'>
  )
endif()

add_dependencies(link_audit_libstdcxx audit_libstdcxx)

add_library(test_audit_libstdcxx INTERFACE)
target_sources(test_audit_libstdcxx INTERFACE audit.c)
target_include_directories(test_audit_libstdcxx INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
