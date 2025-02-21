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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <elf.h>
#include <fcntl.h>
#include <libgen.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "audit_libstdcxx_export.h"
#include "get_libstdcxx_version.h"
#include "macros.h"

#ifndef STATIC
#ifndef GOOGLE_TEST
#define STATIC static
#else
#define STATIC
#endif
#endif

static const char libstdcxx_rel_path[] = "/libstdc++.so.6";
static const size_t len_libstdcxx_rel_path = 15;

STATIC size_t size_t_min(size_t a, size_t b) {
  return (a < b) ? a : b;
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
