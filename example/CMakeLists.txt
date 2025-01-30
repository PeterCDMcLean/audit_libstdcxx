cmake_minimum_required(VERSION 3.21)

project(example_audit_libstdcxx)

# Append special example paths here to the search path for audit library's libstdc++.so.6 smart resolution
set(AuditLibstdcxx_LIBSTDCXX_SO_PATHS "${AuditLibstdcxx_LIBSTDCXX_SO_PATHS} ${CMAKE_CURRENT_SOURCE_DIR}" "$ENV{QUARTUS_ROOTDIR}/linux64"
    CACHE STRING "Set candidate paths for libstdc++.so.6 smart resolution")

set(AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_SO ON CACHE BOOL "Enable auto linking to the libstdcxx.so.6 library")

set(AuditLibstdcxx_AUTO_LINK_LIBSTDCXX_EXE ON CACHE BOOL "Enable auto linking to the link_audit_libstdcxx target for CXX,GNU executable targets")

find_package(AuditLibstdcxx CONFIG REQUIRED)

add_executable(example_audit_libstdcxx)

target_sources(example_audit_libstdcxx PRIVATE test.cpp)

# On purpose, the libstdcxx_so does not have a default install path
# Set it here
install(IMPORTED_RUNTIME_ARTIFACTS AuditLibstdcxx::libstdcxx_so
  LIBRARY DESTINATION lib
)

# Knowing what is going on during link is very useful
target_link_options(example_audit_libstdcxx PRIVATE -Wl,-v)

# The AUDIT_LIBRARIES target property propagates to `target_link_libraries` as a `--audit` linker flag.
# CMake has a bug with Ninja and $ORIGIN escaping and requires extra care to pass $ORIGIN to the command line properly escaped
if ( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
  set_target_properties(example_audit_libstdcxx PROPERTIES AUDIT_LIBRARIES \$$ORIGIN/../lib/$<TARGET_FILE_NAME:AuditLibstdcxx::audit_libstdcxx>)
else()
  set_target_properties(example_audit_libstdcxx PROPERTIES AUDIT_LIBRARIES \$ORIGIN/../lib/$<TARGET_FILE_NAME:AuditLibstdcxx::audit_libstdcxx>)
endif()

# If you want to execute your application from the build directory, the build directory must mimic the expected install
# paths so that DT_AUDIT can find the library properly.

# We must copy the audit library to the AUDIT_LIBRARIES location that we specfy
# This POST_BUILD command does the copying after build
add_custom_command(
  TARGET example_audit_libstdcxx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:AuditLibstdcxx::audit_libstdcxx>
    $<TARGET_FILE_DIR:example_audit_libstdcxx>/lib/$<TARGET_FILE_NAME:AuditLibstdcxx::audit_libstdcxx>
  COMMENT "Copying audit library to ORIGIN"
)

# We copy the exectuable in the build dir to bin/ so that AUDIT_LIBRARIES $ORIGIN/../lib is functional for the BUILD_INTERFACE
add_custom_command(
  TARGET example_audit_libstdcxx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:example_audit_libstdcxx>
    $<TARGET_FILE_DIR:example_audit_libstdcxx>/bin/$<TARGET_FILE_NAME:example_audit_libstdcxx>
  COMMENT "Copying build bin file bin/"
)

# Set up your RPATH/RUNPATH.
# Make sure that the shipped libstdc++.so.6 is available in the RPATH/RUNPATH.
# Otherwise, it will link the system default and all your efforts were in vain!

# Use of RUNPATH vs RPATH is optional, although the recommendation is typically RUNPATH
target_link_options(example_audit_libstdcxx PRIVATE -Wl,--enable-new-dtags)

# We must set RUNPATH in both BUILD and INSTALL if we wish to find the shipped libstdc++
set_target_properties(example_audit_libstdcxx PROPERTIES BUILD_RPATH "$<TARGET_FILE_DIR:AuditLibstdcxx::libstdcxx_so>")
set_target_properties(example_audit_libstdcxx PROPERTIES INSTALL_RPATH "\$ORIGIN:\$ORIGIN/../lib")

install(TARGETS example_audit_libstdcxx EXPORT example_audit_libstdcxx
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# The audit library is installed to the expected DT_AUDIT path
install(FILES $<TARGET_FILE_DIR:example_audit_libstdcxx>/lib/$<TARGET_FILE_NAME:AuditLibstdcxx::audit_libstdcxx> DESTINATION lib)

# Python target creation and installation
get_target_property(LIBSTDCXXSO_PATH AuditLibstdcxx::libstdcxx_so IMPORTED_LOCATION)
get_filename_component(LIBSTDCXXSO_PATH ${LIBSTDCXXSO_PATH} NAME)

# The sitecustomize is generated from a template and the expected libstdcxx path is replaced
# We are starting at pyaudit/sitecustomize.py, then we point to ../lib/${LIBSTDCXXSO_PATH}
set(SITECUSTOMIZE_EXPECTED_LIBSTDCXX_PATH "os.path.join(os.path.dirname(os.path.dirname(__file__)), 'lib', '${LIBSTDCXXSO_PATH}')")
configure_file(${SITECUSTOMIZE_TEMPLATE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/install/pyaudit/sitecustomize.py @ONLY)
add_executable(python_sitecustomize_install IMPORTED GLOBAL)
set_target_properties(python_sitecustomize_install PROPERTIES
  IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/install/pyaudit/sitecustomize.py
)

# The pyaudit scripts are installed using the audit (main script) target and the sitecustomize configured template target created above
install(IMPORTED_RUNTIME_ARTIFACTS AuditLibstdcxx::python_audit_libstdcxx python_sitecustomize_install
  RUNTIME DESTINATION pyaudit
)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pyaudit/elftools
  DESTINATION pyaudit
)
