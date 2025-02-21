enable_testing()
cmake_policy(SET CMP0144 NEW)
include(GoogleTest)

add_executable(tests)
target_sources(tests PRIVATE test.cpp)
target_link_libraries(tests PRIVATE dl audit_libstdcxx_srcs GTest::GTest GTest::Main)
target_link_options(tests PRIVATE "-Wl,-v")
target_compile_definitions(tests PRIVATE GOOGLE_TEST)
gtest_discover_tests(tests)


# Discover the compiler's libstdc++, find the absolute path to the compiler lib dir, then set RPATH to that
# This ensures the compiling libstdc++ is the same as the one that runs it from the build directory
# Note that now the tests binary is not relocatable to other systems without matching gcc paths
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.so
    OUTPUT_VARIABLE LIBSTDCXX_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if(LIBSTDCXX_PATH)
    get_filename_component(COMPILER_LIBSTDCXX_DIR "${LIBSTDCXX_PATH}" DIRECTORY)
    file(REAL_PATH "${COMPILER_LIBSTDCXX_DIR}" ABS_LIBSTDCXX_PATH)
    set_target_properties(tests PROPERTIES BUILD_RPATH "${ABS_LIBSTDCXX_PATH}")
  else()
    message(FATAL_ERROR "Could not determine the path to libstdc++.so")
  endif()
else()
  message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()
