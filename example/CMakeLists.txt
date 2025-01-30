cmake_minimum_required(VERSION 3.21)

project(example_audit_libstdcxx)

find_package(audit_libstdcxx REQUIRED)

add_executable(example_audit_libstdcxx)

target_sources(example_audit_libstdcxx PRIVATE test.cpp)

if (!USE_EXAMPLE_LIBSTDCXX OR "${USE_EXAMPLE_LIBSTDCXX}" STREQUAL "")
  target_link_libraries(example_audit_libstdcxx PRIVATE system_libstdcxx)

  # On purpose, the system_libstdcxx does not have a default install path
  # Set it here
  install(IMPORTED_RUNTIME_ARTIFACTS system_libstdcxx
    LIBRARY DESTINATION lib
  )
  set (OUR_LIBSTDCXX system_libstdcxx)
else()
  add_subdirectory(libstdcxx)

  target_link_libraries(example_audit_libstdcxx PRIVATE example_libstdcxx)

  set (OUR_LIBSTDCXX libstdcxx)
endif()

# Knowing what is going on during link is very useful
target_link_options(example_audit_libstdcxx PRIVATE -Wl,-v)

# The AUDIT_LIBRARIES target property propagates to `target_link_libraries` as a `--audit` linker flag.
# CMake has a bug with Ninja and $ORIGIN escaping and requires extra care to pass $ORIGIN to the command line properly escaped
if ( "${CMAKE_GENERATOR}" STREQUAL "Ninja" )
  set_target_properties(example_audit_libstdcxx PROPERTIES AUDIT_LIBRARIES \$$ORIGIN/../lib/$<TARGET_FILE_NAME:audit_libstdcxx>)
else()
  set_target_properties(example_audit_libstdcxx PROPERTIES AUDIT_LIBRARIES \$ORIGIN/../lib/$<TARGET_FILE_NAME:audit_libstdcxx>)
endif()

# If you want to execute your application from the build directory, the build directory must mimic the expected install
# paths so that DT_AUDIT can find the library properly.

# We must copy the audit library to the AUDIT_LIBRARIES location that we specfy
# This POST_BUILD command does the copying after build
add_custom_command(
  TARGET example_audit_libstdcxx POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:audit_libstdcxx>
    $<TARGET_FILE_DIR:example_audit_libstdcxx>/lib/$<TARGET_FILE_NAME:audit_libstdcxx>
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
set_target_properties(example_audit_libstdcxx PROPERTIES BUILD_RPATH "$<TARGET_FILE_DIR:${OUR_LIBSTDCXX}>")
set_target_properties(example_audit_libstdcxx PROPERTIES INSTALL_RPATH "\$ORIGIN:\$ORIGIN/../lib")

install(TARGETS example_audit_libstdcxx EXPORT example_audit_libstdcxx
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# The audit library is installed to the expected DT_AUDIT path
install(FILES $<TARGET_FILE_DIR:example_audit_libstdcxx>/lib/$<TARGET_FILE_NAME:audit_libstdcxx> DESTINATION lib)

# NOTE: libstdc++ is installed from other cmake files (system_libstcxx or libstdcxx, whichever was chosen for this example)
