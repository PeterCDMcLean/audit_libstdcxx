/*
Copyright (c) 2025 Altera

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"),to deal in the Software without
restriction, including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <link.h>

#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int get_next_SO_path(dl_phdr_info *info, size_t unused, void *p_SO_list) {
  (void)unused;
  std::vector<std::string> &SO_list = *reinterpret_cast<std::vector<std::string> *>(p_SO_list);
  auto p_SO_path = realpath(info->dlpi_name, NULL);
  if (p_SO_path) {
    SO_list.emplace_back(p_SO_path);
    free(p_SO_path);
  }
  return 0;
}

std::vector<std::string> get_SO_realpaths() {
  std::vector<std::string> SO_paths;
  // posix C function to iterate the all the loaded shared objects
  dl_iterate_phdr(get_next_SO_path, &SO_paths);
  return SO_paths;
}

int main(int argc, char **argv) {
  std::cout << "Libraries loaded by this executable:" << std::endl;
  auto SO_paths = get_SO_realpaths();
  for (auto const &SO_path : SO_paths) {
    std::cout << SO_path << std::endl;
  }
  return 0;
}
