# Function: walk_all_targets
# --------------------------
# Iterates over all CMake targets in the project and applies a callback function to each target.
#
# Parameters:
#   callback (IN) - The name of the callback function to invoke for each target.
#
# Behavior:
#   - Uses `spr_get_all_cmake_targets` to retrieve all targets in the project.
#   - Calls the specified callback function for each target using `cmake_language(CALL)`.
#
# Example Usage:
#   function(my_callback target_name)
#     message(STATUS "Processing target: ${target_name}")
#   endfunction()
#
#   walk_all_targets(my_callback)
include_guard(GLOBAL)

function (spr_get_all_cmake_targets out_var current_dir)
  get_property(targets DIRECTORY ${current_dir} PROPERTY BUILDSYSTEM_TARGETS)
  get_property(subdirs DIRECTORY ${current_dir} PROPERTY SUBDIRECTORIES)

  foreach(subdir ${subdirs})
    spr_get_all_cmake_targets(subdir_targets ${subdir})
    list(APPEND targets ${subdir_targets})
  endforeach()

  set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

function(walk_all_targets callback)
  spr_get_all_cmake_targets(all_targets ${CMAKE_SOURCE_DIR})

  foreach(target IN LISTS all_targets)
    cmake_language(CALL ${callback} "${target}")
  endforeach()
endfunction()