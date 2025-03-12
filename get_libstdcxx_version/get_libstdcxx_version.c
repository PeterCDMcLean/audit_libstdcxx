
#include "get_libstdcxx_version.h"

#include <stdint.h>

#include "macros.h"

/**
 *  This simple utility opens the provided libstdc++.so in the first argument and
 *  prints the 32bit hex representation of the version
 */

int main(int argc, char* argv[]) {
  ASSERT(argc == 2, "Number of args should be exactly one\n");
  ASSERT(argv[1] != NULL, "Invalid string arg\n");

  const int fd_libstdcxx = open(argv[1], O_RDONLY);
  ASSERT(fd_libstdcxx >= 0, "Invalid or missing file supplied: %s\n", argv[1]);
  uint32_t version = 0;
  const error_code_t error = get_libstdcxx_version(fd_libstdcxx, argv[1], &version);
  ASSERT(error >= ec_success, "Fatal Error reading supplied libstdc++.so.6: %s\n", argv[1]);
  ASSERT(error == ec_success, "Architecture (32b vs 64b) Error reading supplied libstdc++.so.6: %s\n", argv[1]);
  printf("%08x\n", version);
  // `fd_libstdcxx` is closed by `get_libstdcxx_version`
  return error;
}
