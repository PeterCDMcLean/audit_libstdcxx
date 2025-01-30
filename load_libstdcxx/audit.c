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

#ifndef GOOGLE_TEST
#define STATIC static
#else
#define STATIC
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audit_libstdcxx_export.h"

#define debug_print(fd, prefix, suffix, ...) \
  fprintf(fd, "%s%s:%d:%s(): " suffix, (prefix), ((strlen(__FILE__) < 16) ? __FILE__ : (__FILE__ + strlen(__FILE__) - 16)), __LINE__, __func__, ##__VA_ARGS__)

#define ENABLE_TRACE 0
#define ENABLE_TRACE_ELF 0

#define TRACE(suffix, ...)                                        \
  do {                                                            \
    if (ENABLE_TRACE) {                                           \
      debug_print(stderr, "AUDIT TRACE ", suffix, ##__VA_ARGS__); \
      fflush(stderr);                                             \
    }                                                             \
  } while (0)

#define TRACE_ELF(suffix, ...)                                        \
  do {                                                                \
    if (ENABLE_TRACE_ELF) {                                           \
      debug_print(stderr, "AUDIT TRACE_ELF ", suffix, ##__VA_ARGS__); \
      fflush(stderr);                                                 \
    }                                                                 \
  } while (0)

#define ERROR(suffix, ...) debug_print(stderr, "AUDIT ERROR ", suffix, ##__VA_ARGS__)

// assert can call malloc. Use a macro instead
#define ASSERT(condition, suffix, ...) \
  do {                                 \
    if (!(condition)) {                \
      ERROR(suffix, ##__VA_ARGS__);    \
      abort();                         \
    }                                  \
  } while (0)

static const char libstdcxx_rel_path[] = "/libstdc++.so.6";
static const size_t len_libstdcxx_rel_path = 15;

STATIC uint32_t uint_min(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}
STATIC uint32_t uint_max(uint32_t a, uint32_t b) {
  return (a > b) ? a : b;
}
STATIC size_t size_t_min(size_t a, size_t b) {
  return (a < b) ? a : b;
}

/**
 * Convert an ELF library version A.B.C to an unsigned integer (uint32_t expected)
 * of the form 0x00AABBCC
 * Any A,B, or C that are > 255 are clamped to 255.
 */
STATIC uint32_t version_string_to_int(const char* const str) {
  ASSERT(NULL != str, "Unexpected NULL argument to version string parse");

  uint32_t major_minor_revision[3] = {0};
  uint32_t v = 0;
  for (const char* c = str; *c != '\0'; c++) {
    if ((*c) == '.' && v < 2) {
      v++;
    } else if ((*c) >= '0' && (*c) <= '9') {
      major_minor_revision[v] = (major_minor_revision[v]) * 10 + (uint32_t)((*c) - '0');
    } else {
      break;
    }
  }
  uint32_t version = 0;
  for (int i = 0; i < 3; i++) {
    version = (version << 8) | uint_min(major_minor_revision[i], 255);
  }
  return version;
}

/**
 * Retrieve the dt_runpath or dt_rpath from the parent exectuable's Program Header
 * @return 0 on success, -1 on failure
 */
STATIC int get_parent_executable_runpath_rpath(const ElfW(Phdr) * const phdr, const size_t phnum, const char** const dt_runpath, const char** const dt_rpath) {
  ASSERT(dt_runpath && dt_rpath, "Unexpected NULL arguments\n");

  ASSERT(phdr && phnum, "Failed to retrieve program headers from aux vectors\n");

  // Find the base address by finding PT_PHDR and working backwards
  ElfW(Addr) base_address = 0;

  for (size_t i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_PHDR) {
      base_address = (ElfW(Addr))phdr - phdr[i].p_vaddr;
      break;
    }
  }

  TRACE("base_address %lu\n", (unsigned long)base_address);

  // Iterate over program headers to locate PT_DYNAMIC
  for (size_t i = 0; i < phnum; i++) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      ElfW(Dyn)* dynamic = (ElfW(Dyn)*)(base_address + phdr[i].p_vaddr);
      const char* strtab = NULL;

      // Parse the dynamic section to find DT_RUNPATH and DT_RPATH
      for (ElfW(Dyn)* dyn = dynamic; dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_STRTAB) {
          strtab = (const char*)(dyn->d_un.d_ptr);
        }
      }

      ASSERT(strtab, "No strtab found in program header\n");

      for (ElfW(Dyn)* dyn = dynamic; dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_RUNPATH) {
          *dt_runpath = strtab + dyn->d_un.d_val;
          TRACE("DT_RUNPATH: %s\n", strtab + dyn->d_un.d_val);
        } else if (dyn->d_tag == DT_RPATH) {
          *dt_rpath = strtab + dyn->d_un.d_val;
          TRACE("DT_RPATH: %s\n", strtab + dyn->d_un.d_val);
        }
      }
      break;
    }
  }
  return 0;
}

/**
 * This function checks whether a path exists and returns the _open_ fd in the void* data.
 * The caller is responsible for closing the open fd.
 * A callback is used to facilitate unit testing
 * @return 0 on success -1 on failure
 */
STATIC int trypath(const char* const path, void* data) {
  ASSERT(NULL != data && NULL != path, "Unexpected NULL argument");
  // Check if the path exists
  TRACE("Trying path %s\n", path);
  int fd = open(path, O_RDONLY);

  // File does not exist
  if (fd < 0) {
    return -1;
  }

  // Store the fd in data pointer, to be used by the caller
  *((int*)data) = fd;

  return 0;
}

/**
 * From a DT_RUNPATH or DT_RPATH, find the first path that contains libstdc++.so.6
 * DT_RUNPATH / DT_PATH are colon separated list of directories to search for dependencies
 * ex:  "$ORIGIN:$ORIGIN../lib"
 * Replace $ORIGIN as necessary
 * NOTE: on a success, the caller of this function owns the buffer at *p_path and must munmap as necessary
 * @return 0 on success -1 on failure
 */
STATIC int find_libstdcxx_from_dt_path(const char* const dt_path, const char* const ORIGIN, int (*trypath_callback)(const char* const path, void* data),
                                       void* callback_data, char** p_path, size_t* p_path_buffer_len) {
  ASSERT(p_path && p_path_buffer_len, "Unexpected NULL arguments");

  const size_t dt_path_len = strlen(dt_path);
  const size_t len_ORIGIN = strlen(ORIGIN);

  char* libstdcxx_path = NULL;
  size_t len_path_buffer = 0;
  *p_path = NULL;
  *p_path_buffer_len = 0;

  const char* dt_path_cursor = dt_path;

  TRACE("dt_path %s\n", dt_path);
  TRACE("dt_path_len %lu\n", (unsigned long)dt_path_len);

  while (dt_path_cursor < (dt_path + dt_path_len)) {
    const char* end_of_this_section = dt_path_cursor;
    // Find the termination of this section. (Either the ':' or '\0')
    // clang-format off
    for(;
      (end_of_this_section < (dt_path + dt_path_len)) &&
      *end_of_this_section != ':' &&
      *end_of_this_section != '\0'; end_of_this_section++) {

    }
    // clang-format on

    size_t len_section = (size_t)(end_of_this_section - dt_path_cursor);
    if (len_section == 0) {
      dt_path_cursor++;
      continue;
    }

    // Construct a path to libstdc++
    TRACE("This section and on %s\n", dt_path_cursor);
    TRACE("section len %lu\n", (unsigned long)len_section);
    if (0 == strncmp("$ORIGIN", dt_path_cursor, 7)) {
      TRACE("substitute $ORIGIN\n");

      // Allocate space for the follow path
      // $ORIGIN/section_path/libstdc++\0
      size_t needed_len = len_ORIGIN + (len_section - 7) + len_libstdcxx_rel_path + 1;
      if (needed_len > len_path_buffer) {
        if (len_path_buffer) {
          munmap(libstdcxx_path, len_path_buffer);
        }
        libstdcxx_path = (char*)mmap(NULL, needed_len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (libstdcxx_path == MAP_FAILED) {
          ERROR("Audit library: Failed to allocate memory for libstdc++ path. runtime link errors may occur\n");
          return -1;
        }
        len_path_buffer = needed_len;
      }
      // Truncate
      libstdcxx_path[0] = '\0';

      // Construct the string
      strncat(libstdcxx_path, ORIGIN, len_path_buffer - 1);
      libstdcxx_path[len_path_buffer - 1] = '\0';
      dt_path_cursor += 7;
      len_section -= 7;
      if (len_section > 0) {
        strncat(libstdcxx_path, dt_path_cursor, size_t_min(len_section, len_path_buffer - 1 - len_ORIGIN));
        libstdcxx_path[len_path_buffer - 1] = '\0';
      }
      strncat(libstdcxx_path, libstdcxx_rel_path, len_path_buffer - 1 - len_ORIGIN - len_section);
      libstdcxx_path[len_path_buffer - 1] = '\0';

    } else {
      size_t needed_len = len_section + len_libstdcxx_rel_path + 1;
      if (needed_len > len_path_buffer) {
        if (len_path_buffer) {
          munmap(libstdcxx_path, len_path_buffer);
        }
        libstdcxx_path = (char*)mmap(NULL, needed_len, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (libstdcxx_path == MAP_FAILED) {
          ERROR("Audit library: Failed to allocate memory for libstdc++ path. runtime link errors may occur\n");
          return -1;
        }
        len_path_buffer = needed_len;
      }
      libstdcxx_path[0] = '\0';
      strncat(libstdcxx_path, dt_path_cursor, size_t_min(len_section, len_path_buffer - 1));
      libstdcxx_path[len_path_buffer - 1] = '\0';
      strncat(libstdcxx_path, libstdcxx_rel_path, len_path_buffer - 1 - len_section);
      libstdcxx_path[len_path_buffer - 1] = '\0';
    }

    // Check if the path at `shipped_libstdcxx_path` exists
    TRACE("Trying path %s\n", libstdcxx_path);
    if (0 == trypath_callback(libstdcxx_path, callback_data)) {
      // found libstdc++ in RUNPATH/RPATH. Can return indicating success

      // Return the path and length of the buffer
      *p_path = libstdcxx_path;
      *p_path_buffer_len = len_path_buffer;

      // No munmap occurs! The caller now owns the buffer
      return 0;
    }

    dt_path_cursor = end_of_this_section + 1;
  }

  // Free
  if (NULL != libstdcxx_path) {
    munmap(libstdcxx_path, len_path_buffer);
  }

  // Did not find a libstdc++ library in dt_path
  return -1;
}

/**
 * Function to extract the glibcxx version from a libstdc++ shared library
 * Examines the .gnu.version_d section to find the max version of the version strings of the form GLIBCXX_Major.Minor.Revision
 * Versions returned are of the form 0x00AABBCC
 * @return 0 on success, -1 on failure
 */
STATIC int get_libstdcxx_version(const int fd, const char* const filename, uint32_t* const glibcxx_version) {
  ASSERT(fd >= 0, "Expecting an open file descriptor");
  ASSERT(filename && glibcxx_version, "Unexpected NULL arguments");

  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("Error getting file size");
    close(fd);
    return -1;
  }

  size_t file_size = (size_t)st.st_size;
  void* mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    perror("Error mapping file");
    close(fd);
    return -1;
  }

  close(fd);

  // Check ELF magic bytes
  ElfW(Ehdr)* ehdr = (ElfW(Ehdr)*)mapped;
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    ERROR("File %s is not a valid ELF file\n", filename);
    munmap(mapped, file_size);
    return -1;
  }

  ASSERT(ehdr->e_shoff < file_size, "Invalid ELF, section header is outside the ELF size\n");

  // Locate section headers
  ElfW(Shdr)* shdr = (ElfW(Shdr)*)((char*)mapped + ehdr->e_shoff);

  // Bounds checking
  ASSERT(ehdr->e_shstrndx < (file_size - ehdr->e_shoff), "Invalid ELF, section header table index is outside the ELF size\n");
  ASSERT(ehdr->e_shstrndx < ehdr->e_shnum, "Invalid ELF, section header table index is outside the e_shnum\n");
  ASSERT(shdr[ehdr->e_shstrndx].sh_offset < file_size, "Invalid ELF, section header table is outside the ELF size\n");
  ASSERT(ehdr->e_shnum <= (file_size / sizeof(ElfW(Shdr))), "Invalid ELF, num of section headers is beyond the ELF size\n");

  // Section header string table
  const char* shstrtab = (char*)mapped + shdr[ehdr->e_shstrndx].sh_offset;

  const ElfW(Xword) size_section_header_string_table = shdr[ehdr->e_shstrndx].sh_size;

  ASSERT(size_section_header_string_table < (file_size - shdr[ehdr->e_shstrndx].sh_offset), "Invalid ELF, string table is beyond the ELF size\n");

  int at_least_one_glibcxx_version_found = 0;

  // Iterate through sections to find .gnu.version_d
  for (int i = 0; i < ehdr->e_shnum; i++) {
    ASSERT(shdr[i].sh_name < size_section_header_string_table, "Invalid ELF, section header string table index is out of range\n");
    if (strncmp(&shstrtab[shdr[i].sh_name], ".gnu.version_d", 14) == 0) {
      TRACE_ELF("Found .gnu.version_d\n");
      TRACE_ELF("sh_name      = %lx\n", (long unsigned int)shdr[i].sh_name);      /* Section name (string tbl index) */
      TRACE_ELF("sh_type      = %lx\n", (long unsigned int)shdr[i].sh_type);      /* Section type */
      TRACE_ELF("sh_flags     = %lx\n", (long unsigned int)shdr[i].sh_flags);     /* Section flags */
      TRACE_ELF("sh_addr      = %lx\n", (long unsigned int)shdr[i].sh_addr);      /* Section virtual addr at execution */
      TRACE_ELF("sh_offset    = %lx\n", (long unsigned int)shdr[i].sh_offset);    /* Section file offset */
      TRACE_ELF("sh_size      = %lx\n", (long unsigned int)shdr[i].sh_size);      /* Section size in bytes */
      TRACE_ELF("sh_link      = %lx\n", (long unsigned int)shdr[i].sh_link);      /* Link to another section */
      TRACE_ELF("sh_info      = %lx\n", (long unsigned int)shdr[i].sh_info);      /* Additional section information */
      TRACE_ELF("sh_addralign = %lx\n", (long unsigned int)shdr[i].sh_addralign); /* Section alignment */
      TRACE_ELF("sh_entsize   = %lx\n", (long unsigned int)shdr[i].sh_entsize);   /* Entry size if section holds table */

      // Found .gnu.version_d
      ASSERT(shdr[i].sh_offset < file_size, "Invalid ELF, .gnu.version_d verdef offset is outside the ELF size\n");
      ElfW(Verdef)* verdef_base = (ElfW(Verdef)*)((char*)mapped + shdr[i].sh_offset);
      ElfW(Verdef)* verdef = verdef_base;

      TRACE_ELF("verdef_base = %lx\n", (unsigned long)verdef_base);
      TRACE_ELF("verdef = %lx\n", (unsigned long)verdef);

      while ((verdef >= verdef_base) && ((ElfW(Xword))((char*)verdef - (char*)verdef_base) < shdr[i].sh_size)) {
        TRACE_ELF("verdef->vd_version = %lx\n", (long unsigned int)verdef->vd_version); /* Version revision */
        TRACE_ELF("verdef->vd_flags   = %lx\n", (long unsigned int)verdef->vd_flags);   /* Version information */
        TRACE_ELF("verdef->vd_ndx     = %lx\n", (long unsigned int)verdef->vd_ndx);     /* Version Index */
        TRACE_ELF("verdef->vd_cnt     = %lx\n", (long unsigned int)verdef->vd_cnt);     /* Number of associated aux entries */
        TRACE_ELF("verdef->vd_hash    = %lx\n", (long unsigned int)verdef->vd_hash);    /* Version name hash value */
        TRACE_ELF("verdef->vd_aux     = %lx\n", (long unsigned int)verdef->vd_aux);     /* Offset in bytes to verdaux array */
        TRACE_ELF("verdef->vd_next    = %lx\n", (long unsigned int)verdef->vd_next);    /* Offset in bytes to next verdef*/

        ElfW(Verdaux)* verdaux = (ElfW(Verdaux)*)((char*)verdef + verdef->vd_aux);

        ASSERT((void*)verdaux < (mapped+file_size), "Invalid ELF, verdaux pointer is outside the ELF size\n");
        ASSERT(shdr[i].sh_offset < file_size, "Invalid ELF, verdef offset is outside the ELF size\n");
        char* string_table = (char*)mapped + shdr[shdr[i].sh_link].sh_offset;

        for (int vdaux_i = 0; vdaux_i < verdef->vd_cnt; vdaux_i++) {
          char* vdaux_name = (string_table + verdaux->vda_name);
          ASSERT((void*)vdaux_name < (mapped+file_size), "Invalid ELF, vdaux_name is outside the ELF size\n");
          if (0 == strncmp(vdaux_name, "GLIBCXX_", 8)) {
            const uint32_t version = version_string_to_int(vdaux_name + 8);
            if (!at_least_one_glibcxx_version_found) {
              *glibcxx_version = version;
              at_least_one_glibcxx_version_found = 1;
            } else {
              *glibcxx_version = uint_max(*glibcxx_version, version);
            }
            TRACE_ELF("Version %s i %x\n", vdaux_name, version);
            TRACE_ELF("*glibcxx_version = %x\n",*glibcxx_version);
          }

          verdaux = (ElfW(Verdaux)*)(char*)verdaux + verdaux->vda_next;
        }
        if (0 == verdef->vd_next) {
          break;
        }
        verdef = (ElfW(Verdef)*)((char*)verdef + verdef->vd_next);

        TRACE_ELF("verdef = %lx\n", (unsigned long)verdef);
      }
    }
  }

  munmap(mapped, file_size);
  if (!at_least_one_glibcxx_version_found) {
    return -1;
  }
  return 0;
}

static uint32_t shipped_glibcxx_version = 0xDEADBEEF;
static char* shipped_libstdcxx_path = NULL;
static size_t len_shipped_path_buffer = 0;

/**
 * la_version is called exactly once by ld.so before any other action by the loader
 * We use this to initialize the static variables above
 * @return LAV_CURRENT will indicate to the loader which audit library interface it is compiled for
 */
AUDIT_LIBSTDCXX_EXPORT unsigned int la_version(unsigned int version) {
  // Version argument is not used (except for TRACE)
  (void)version;

  TRACE("la_version(): version = %u; LAV_CURRENT = %u\n", version, LAV_CURRENT);

  ASSERT(len_libstdcxx_rel_path == strlen(libstdcxx_rel_path), "String / length constant mismatch");

  // Return the ORIGIN (path of the executable)
  char* ORIGIN = (char*)getauxval(AT_EXECFN);
  TRACE("aux origin %s\n", ORIGIN);

  // Copy the ORIGIN path in order to strip the executable filename and leave the base path
  size_t len_ORIGIN = strlen(ORIGIN);
  char* origin_local = (char*)mmap(NULL, len_ORIGIN, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (origin_local == MAP_FAILED) {
    ERROR("Audit library: Failed to allocate memory for libstdc++ path. runtime link errors may occur\n");
    return LAV_CURRENT;
  }
  memcpy(origin_local, ORIGIN, len_ORIGIN);

  // Strip the filename to get the basepath of the executable
  ORIGIN = dirname(origin_local);

  TRACE("Audit library: ORIGIN at %s\n", ORIGIN);

  // From the aux vectors, we can get the program header, which links to the dynamic section, which has DT_RUNPATH and DT_PATH (if they exist)
  const char no_path = '\0';
  const char* dt_runpath = &no_path;
  const char* dt_rpath = &no_path;

  // Retrieve program headers and their count from auxiliary vectors
  ElfW(Phdr)* phdr = (ElfW(Phdr)*)getauxval(AT_PHDR);
  size_t phnum = getauxval(AT_PHNUM);
  int error = get_parent_executable_runpath_rpath(phdr, phnum, &dt_runpath, &dt_rpath);
  if (0 != error) {
    ERROR("Audit library: Cannot find our libstdc++. runtime link errors may occur\n");
    return LAV_CURRENT;
  }
  TRACE("DT_RUNPATH %s\n", dt_runpath);
  TRACE("DT_RPATH %s\n", dt_rpath);

  // Look in DT_RUNPATH then DT_RPATH for libstdc++ and record its path
  int found = -1;
  int fd_libstdcxx = -1;
  if (dt_runpath[0] != '\0') {
    found = find_libstdcxx_from_dt_path(dt_runpath, ORIGIN, &trypath, (void*)&fd_libstdcxx, &shipped_libstdcxx_path, &len_shipped_path_buffer);
  }
  if (0 != found) {
    found = find_libstdcxx_from_dt_path(dt_rpath, ORIGIN, &trypath, (void*)&fd_libstdcxx, &shipped_libstdcxx_path, &len_shipped_path_buffer);
  }
  if (0 != found) {
    ERROR("Audit library: Cannot find our libstdc++. runtime link errors may occur\n");
    if (NULL != shipped_libstdcxx_path) {
      munmap(shipped_libstdcxx_path, len_shipped_path_buffer);
    }
    munmap(ORIGIN, len_ORIGIN);
    return LAV_CURRENT;
  }

  if (shipped_libstdcxx_path) {
    TRACE("Our shipped libstdc++ at %s\n", shipped_libstdcxx_path);
  }

  // We found libstdc++, record its versions
  if (get_libstdcxx_version(fd_libstdcxx, shipped_libstdcxx_path, &shipped_glibcxx_version)) {
    ERROR("Audit library: Could not determine shipped libstdc++ version");
  }
  munmap(ORIGIN, len_ORIGIN);

  TRACE("Our version is %x?\n", shipped_glibcxx_version);

  return LAV_CURRENT;
}

/**
 * la_objsearch is called by the loader as it attempts to resolve the library.
 * The flag denotes what type of path prefix is used
 * If the library being searched for is libstdc++, we examine its version and allow it
 * to be loaded ONLY if the glibcxx version is greater or equal than the shipped version
 */
AUDIT_LIBSTDCXX_EXPORT char* la_objsearch(const char* name, uintptr_t* cookie, unsigned int flag) {
  // cookie argument is not used.
  (void)cookie;
  // flag argument is not used.
  (void)flag;

  TRACE("la_objsearch(): name = %s; cookie = %p\n", name, cookie);
  TRACE("; flag = %s\n", (flag == LA_SER_ORIG)      ? "LA_SER_ORIG"
                         : (flag == LA_SER_LIBPATH) ? "LA_SER_LIBPATH"
                         : (flag == LA_SER_RUNPATH) ? "LA_SER_RUNPATH"
                         : (flag == LA_SER_DEFAULT) ? "LA_SER_DEFAULT"
                         : (flag == LA_SER_CONFIG)  ? "LA_SER_CONFIG"
                         : (flag == LA_SER_SECURE)  ? "LA_SER_SECURE"
                                                    : "???");

  // This condition means the initial check to find the shipped libstdc++ versions failed.
  // early 'exit' by releasing the path back to ld.so
  if ((shipped_glibcxx_version == 0xDEADBEEF) || (shipped_libstdcxx_path == NULL)) {
    TRACE("Earlier error, exit early\n");
    return (char*)name;
  }

  size_t len = strlen(name);

  // Compare the suffix of the path to see if this search is for libstdc++
  // If not, release the path back to ld.so
  if ((len < len_libstdcxx_rel_path) || (0 != strncmp(libstdcxx_rel_path, name + len - len_libstdcxx_rel_path, len_libstdcxx_rel_path))) {
    return (char*)name;
  }
  // At this point, we know we are searching for a libstdc++

  // We only examine NON runpath as the 'system' versions. We already parsed RUNPATH/RPATH for libstdc++
  if (flag == LA_SER_RUNPATH) {
    return (char*)NULL;
  }

  // Check if the file exists
  int fd_system_libstdcxx = open(name, O_RDONLY);

  // The library does not exist at this path (or we encountered another error), release it
  if (fd_system_libstdcxx < 0) {
    return (char*)name;
  }

  TRACE("File %s exists\n", name);

  // Should we automatically accept LD_LIBRARY_PATH entries, regardless if they have the correct version?
  // Probably not
  // if (flag == LA_SER_LIBPATH) {
  //   return (char*)name;
  // }

  // Once we have found a valid libstdc++.so.6, we compare the shipped vs system version
  // We load whichever is higher version.
  // This search path exists, extract the version of this system libstdc++ library
  uint32_t system_glibcxx_version = 0;
  if (get_libstdcxx_version(fd_system_libstdcxx, name, &system_glibcxx_version)) {
    ERROR("Audit library: Error reading system libstdc++ version");
  }

  TRACE("System glibcxx %x shipped %x\n", system_glibcxx_version, shipped_glibcxx_version);

  // If the searched system library version is lower than the shipped version, use the system library
  if (system_glibcxx_version < shipped_glibcxx_version) {
    TRACE("System glibcxx %x is less than shipped %x. Skipping\n", system_glibcxx_version, shipped_glibcxx_version);

    return shipped_libstdcxx_path;
  }

  // This system version is greater than the shipped version. We overwrite the global version variables and allow the
  // ld.so to choose the system library we're currently evaluating.
  shipped_glibcxx_version = system_glibcxx_version;
  return (char*)name;
}

/**
 * Free our path buffer once the library loading has completed.
 * LA_ACT_CONSISTENT happens after all paths have been searched and all libraries loaded
 */
AUDIT_LIBSTDCXX_EXPORT void la_activity(uintptr_t* cookie, unsigned int flag) {
  // Unused arguments
  (void)cookie;
  (void)flag;
  if ((LA_ACT_CONSISTENT == flag) && (NULL != shipped_libstdcxx_path)) {
    munmap(shipped_libstdcxx_path, len_shipped_path_buffer);
    shipped_libstdcxx_path = NULL;
  }
  TRACE("la_activity(): cookie = %p; flag = %s\n", cookie,
        (flag == LA_ACT_CONSISTENT) ? "LA_ACT_CONSISTENT"
        : (flag == LA_ACT_ADD)      ? "LA_ACT_ADD"
        : (flag == LA_ACT_DELETE)   ? "LA_ACT_DELETE"
                                    : "???");
}

// Unused audit library functions. Left here as a reference for the future.
#if 0
AUDIT_LIBSTDCXX_EXPORT unsigned int la_objopen(struct link_map *map, Lmid_t lmid, uintptr_t *cookie) {
  printf("la_objopen(): loading \"%s\"; lmid = %s; cookie=%p\n",
          map->l_name,
          (lmid == LM_ID_BASE) ?  "LM_ID_BASE" :
          (lmid == LM_ID_NEWLM) ? "LM_ID_NEWLM" :
          "???",
          cookie);

  return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}

AUDIT_LIBSTDCXX_EXPORT unsigned intla_objclose (uintptr_t *cookie) {
  printf("la_objclose(): %p\n", cookie);

  return 0;
}

AUDIT_LIBSTDCXX_EXPORT void la_preinit(uintptr_t *cookie) {
  printf("la_preinit(): %p\n", cookie);
}

AUDIT_LIBSTDCXX_EXPORT uintptr_t la_symbind32(Elf32_Sym *sym, unsigned int ndx, uintptr_t *refcook, uintptr_t *defcook, unsigned int *flags, const char *symname) {
  printf("la_symbind32(): symname = %s; sym->st_value = %p\n",
          symname, sym->st_value);
  printf("        ndx = %u; flags = %#x", ndx, *flags);
  printf("; refcook = %p; defcook = %p\n", refcook, defcook);

  return sym->st_value;
}

AUDIT_LIBSTDCXX_EXPORT uintptr_t la_symbind64(Elf64_Sym *sym, unsigned int ndx, uintptr_t *refcook, uintptr_t *defcook, unsigned int *flags, const char *symname) {
  printf("la_symbind64(): symname = %s; sym->st_value = %p\n",
          symname, sym->st_value);
  printf("        ndx = %u; flags = %#x", ndx, *flags);
  printf("; refcook = %p; defcook = %p\n", refcook, defcook);

  return sym->st_value;
}

AUDIT_LIBSTDCXX_EXPORT Elf64_Addr la_x86_64_gnu_pltenter(Elf64_Sym *sym, unsigned int ndx, uintptr_t *refcook, uintptr_t *defcook,
                                  La_x86_64_regs *regs, unsigned int *flags, const char *symname, long *framesizep) {
  printf("pltenter(): %s (%p)\n", symname, sym->st_value);

  return sym->st_value;
}

AUDIT_LIBSTDCXX_EXPORT unsigned int la_x86_64_gnu_pltexit(Elf64_Sym *__sym, unsigned int __ndx, uintptr_t *__refcook, uintptr_t *__defcook,
                                   const La_x86_64_regs *__inregs, La_x86_64_retval *__outregs, const char *symname) {
  return 0;
}

#endif
