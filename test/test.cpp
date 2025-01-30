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
#include <iostream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>

extern "C" {
#include <link.h>
#include "error_types.h"
uint32_t version_string_to_int(const char* const str);
error_code_t get_parent_executable_runpath_rpath(const ElfW(Phdr) * const phdr, const size_t phnum, const char** const dt_runpath, const char** const dt_rpath);
error_code_t get_libstdcxx_version(const int fd, const char* const filename, uint32_t* const glibcxx_version);
error_code_t find_libstdcxx_from_dt_path(const char* const dt_path, const char* const ORIGIN, error_code_t (*trypath_callback)(const char* const path, void* data),
                                void* callback_data, char** p_path, size_t* p_path_buffer_len);
}

TEST(VerStr2Int, empty) {
  EXPECT_EQ(version_string_to_int(""), 0);
}

TEST(VerStr2Int, one_dot) {
  EXPECT_EQ(version_string_to_int("."), 0);
}
TEST(VerStr2Int, two_dot) {
  EXPECT_EQ(version_string_to_int(".."), 0);
}
TEST(VerStr2Int, three_dot) {
  EXPECT_EQ(version_string_to_int("..."), 0);
}
TEST(VerStr2Int, four_dot) {
  EXPECT_EQ(version_string_to_int("...."), 0);
}
TEST(VerStr2Int, num_dot) {
  EXPECT_EQ(version_string_to_int("1."), 0x00010000);
}
TEST(VerStr2Int, num_dot_num) {
  EXPECT_EQ(version_string_to_int("1.2"), 0x00010200);
}
TEST(VerStr2Int, num_dot_num_dot) {
  EXPECT_EQ(version_string_to_int("1.2."), 0x00010200);
}
TEST(VerStr2Int, num_dot_num_dot_num) {
  EXPECT_EQ(version_string_to_int("1.2.3"), 0x00010203);
}
TEST(VerStr2Int, overflow) {
  EXPECT_EQ(version_string_to_int("512.256.255"), 0x00FFFFFF);
}
TEST(VerStr2Int, num_dot_num_dot_num_dot_num) {
  EXPECT_EQ(version_string_to_int("1.2.3.4"), 0x00010203);
}

extern "C" error_code_t cpptrypath_callback(const char* const path, void* data);

struct callback_data_t {
  std::string match_path;
  std::vector<std::string> paths;
  callback_data_t() = delete;
  callback_data_t(std::string match_path) : match_path(std::move(match_path)), paths() {
  }

  size_t get_max_path_len() {
    size_t max_path_len = 0;
    for (auto& path : paths) {
      max_path_len = std::max(max_path_len, path.length());
      if (path == match_path) {
        break;
      }
    }
    return max_path_len;
  }
};

error_code_t cpptrypath_callback(const char* const path, void* data) {
  callback_data_t* cdata = reinterpret_cast<callback_data_t*>(data);
  cdata->paths.push_back(std::string(path));
  if (cdata->match_path == std::string(path)) {
    return ec_success;
  }
  return ec_fatal_error;
}

TEST(ParseDTPath, empty) {
  char* path;
  size_t path_buffer_len;
  callback_data_t data("");
  EXPECT_EQ(find_libstdcxx_from_dt_path("", "orangin", &cpptrypath_callback, (void*)&data, &path, &path_buffer_len), ec_fatal_error);
  EXPECT_EQ(data.paths.size(), 0);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(path_buffer_len, 0);
  if (path) {
    munmap(path, path_buffer_len);
  }
}

TEST(ParseDTPath, only_semicolon) {
  char* path = reinterpret_cast<char*>(1);
  size_t path_buffer_len = -1;
  callback_data_t data("");
  EXPECT_EQ(find_libstdcxx_from_dt_path(":", "orangin", &cpptrypath_callback, &data, &path, &path_buffer_len), ec_fatal_error);
  EXPECT_EQ(data.paths.size(), 0);
  EXPECT_EQ(path, nullptr);
  EXPECT_EQ(path_buffer_len, 0);
  if (path) {
    munmap(path, path_buffer_len);
  }
}

TEST(ParseDTPath, ORIGIN) {
  char* path;
  size_t path_buffer_len;
  callback_data_t data("orangin/libstdc++.so.6");
  EXPECT_EQ(find_libstdcxx_from_dt_path("$ORIGIN", "orangin", &cpptrypath_callback, &data, &path, &path_buffer_len), ec_success);
  ASSERT_EQ(data.paths.size(), 1);
  EXPECT_EQ(data.paths.at(0), "orangin/libstdc++.so.6");
  EXPECT_EQ(std::string(path), "orangin/libstdc++.so.6");
  EXPECT_EQ(path_buffer_len, data.get_max_path_len() + 1);
  if (path) {
    munmap(path, path_buffer_len);
  }
}

TEST(ParseDTPath, ORIGIN_2) {
  char* path;
  size_t path_buffer_len;
  callback_data_t data("orangin/../libstdc++.so.6");
  EXPECT_EQ(find_libstdcxx_from_dt_path("$ORIGIN:$ORIGIN/..", "orangin", &cpptrypath_callback, &data, &path, &path_buffer_len), ec_success);
  ASSERT_EQ(data.paths.size(), 2);
  EXPECT_EQ(data.paths.at(0), "orangin/libstdc++.so.6");
  EXPECT_EQ(data.paths.at(1), "orangin/../libstdc++.so.6");
  EXPECT_EQ(std::string(path), "orangin/../libstdc++.so.6");
  EXPECT_EQ(path_buffer_len, data.get_max_path_len() + 1);
  if (path) {
    munmap(path, path_buffer_len);
  }
}

TEST(ParseDTPath, ORIGIN_2_1) {
  char* path;
  size_t path_buffer_len;
  callback_data_t data("orangin/libstdc++.so.6");
  EXPECT_EQ(find_libstdcxx_from_dt_path("$ORIGIN:$ORIGIN/..", "orangin", &cpptrypath_callback, &data, &path, &path_buffer_len), ec_success);
  ASSERT_EQ(data.paths.size(), 1);
  EXPECT_EQ(data.paths.at(0), "orangin/libstdc++.so.6");
  EXPECT_EQ(std::string(path), "orangin/libstdc++.so.6");
  EXPECT_EQ(path_buffer_len, data.get_max_path_len() + 1);
  if (path) {
    munmap(path, path_buffer_len);
  }
}

// clang-format off
const std::map<std::string, std::string> gcc_ver_to_abi = {
  { "3.1.0", "3.1"  },
  { "3.1.1", "3.1"  },
  { "3.2.0", "3.2"  },
  { "3.2.1", "3.2.1"},
  { "3.2.2", "3.2.2"},
  { "3.2.3", "3.2.2"},
  { "3.3.0", "3.2.2"},
  { "3.3.1", "3.2.3"},
  { "3.3.2", "3.2.3"},
  { "3.3.3", "3.2.3"},
  { "3.4.0", "3.4"  },
  { "3.4.1", "3.4.1"},
  { "3.4.2", "3.4.2"},
  { "3.4.3", "3.4.3"},
  { "4.0.0", "3.4.4"},
  { "4.0.1", "3.4.5"},
  { "4.0.2", "3.4.6"},
  { "4.0.3", "3.4.7"},
  { "4.1.1", "3.4.8"},
  { "4.2.0", "3.4.9"},
  { "4.3.0", "3.4.10"},
  { "4.4.0", "3.4.11"},
  { "4.4.1", "3.4.12"},
  { "4.4.2", "3.4.13"},
  { "4.5.0", "3.4.14"},
  { "4.6.0", "3.4.15"},
  { "4.6.1", "3.4.16"},
  { "4.7.0", "3.4.17"},
  { "4.8.0", "3.4.18"},
  { "4.8.3", "3.4.19"},
  { "4.9.0", "3.4.20"},
  { "5.1.0", "3.4.21"},
  { "6.1.0", "3.4.22"},
  { "7.1.0", "3.4.23"},
  { "7.2.0", "3.4.24"},
  { "8.1.0", "3.4.25"},
  { "9.1.0", "3.4.26"},
  { "9.2.0", "3.4.27"},
  { "9.3.0", "3.4.28"},
  {"10.1.0", "3.4.28"},
  {"11.1.0", "3.4.29"},
  {"12.1.0", "3.4.30"},
  {"13.1.0", "3.4.31"},
  {"13.2.0", "3.4.32"},
  {"14.1.0", "3.4.33"}
};
// clang-format on

std::string getLibstdcppPath() {
  // Try to open the libstdc++ shared library
  void* handle = dlopen("libstdc++.so.6", RTLD_LAZY);
  if (!handle) {
    throw std::runtime_error("Failed to load libstdc++.so.6: " + std::string(dlerror()));
  }

  // Use dlinfo to get the shared library's file path
  struct link_map* linkMap = nullptr;
  if (dlinfo(handle, RTLD_DI_LINKMAP, &linkMap) != 0) {
    dlclose(handle);
    throw std::runtime_error("Failed to get link map: " + std::string(dlerror()));
  }

  // Extract the path from the link_map structure
  std::string libraryPath = linkMap->l_name;

  // Close the library handle
  dlclose(handle);

  return libraryPath;
}

TEST(ParseElf, CheckThisGcc) {
#define xstr(a) str(a)
#define str(a) #a
  const std::string gcc_version(xstr(__GNUC__) "." xstr(__GNUC_MINOR__) "." xstr(__GNUC_PATCHLEVEL__));
#undef xstr
#undef str

  ASSERT_GT(gcc_ver_to_abi.count(gcc_version), 0) << "Unknown gcc version " << gcc_version << ". The version to ABI mapping must be added" << std::endl;
  const uint32_t this_gcc_abi_ver_int = version_string_to_int(gcc_ver_to_abi.at(gcc_version).c_str());
  std::string libstdcxx_path = getLibstdcppPath();
  uint32_t elf_abi_version = 0;
  int fd = open(libstdcxx_path.c_str(), O_RDONLY);

  ASSERT_GT(fd, 0) << "Error opening libstdcc++ path\n";
  ASSERT_EQ(get_libstdcxx_version(fd, libstdcxx_path.c_str(), &elf_abi_version), ec_success);
  EXPECT_EQ(this_gcc_abi_ver_int, elf_abi_version)
      << "This test can fail if the tests are compiled with one gcc version and run with a libstdc++.so.6 from a different gcc version" << std::endl
      << "Ensure that the gcc used to compile this test matches the libstdc++.so.6 used to run it" << std::endl;
}
