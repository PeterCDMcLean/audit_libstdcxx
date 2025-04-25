# Function: custom_transitive_build_rpath
# ---------------------------------------
# Iterates over all CMake targets in the project and appends a custom transitive `BUILD_RPATH_` property
# to the target's BUILD_RPATH property
#
# Behavior:
#   - Uses the `walk_all_targets` function to iterate over all targets in the project.
#   - Invokes `apply_transitive_rpath_to_lib_and_exe` for each target to apply the `BUILD_RPATH` property.
#
# Example Usage:
#   custom_transitive_build_rpath()

include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/walk_all_targets.cmake")

function(apply_transitive_rpath_to_lib_and_exe target)
  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "SHARED_LIBRARY" OR target_type STREQUAL "MODULE_LIBRARY" OR target_type STREQUAL "EXECUTABLE")
    get_target_property(tgt_build_rpath ${target} BUILD_RPATH_)
    if (NOT tgt_build_rpath OR "${tgt_build_rpath}" STREQUAL "tgt_build_rpath-NOTFOUND")
      set(tgt_build_rpath "$<TARGET_PROPERTY:${target},BUILD_RPATH_>")
    else()
      list(APPEND tgt_build_rpath "$<TARGET_PROPERTY:${target},BUILD_RPATH_>")
    endif()
    set_target_properties(${target} PROPERTIES
      BUILD_RPATH "${tgt_build_rpath}"
    )
  endif()
endfunction()

function(custom_transitive_build_rpath)
  cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL walk_all_targets "apply_transitive_rpath_to_lib_and_exe")
endfunction()


