#ifndef _GET_LIBSTDCXX_VERSION_H_
#define _GET_LIBSTDCXX_VERSION_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "macros.h"
#include "error_types.h"

#ifndef STATIC
#ifndef GOOGLE_TEST
#define STATIC static
#else
#define STATIC
#endif
#endif

STATIC uint32_t uint_min(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}
STATIC uint32_t uint_max(uint32_t a, uint32_t b) {
  return (a > b) ? a : b;
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
 * Function to extract the glibcxx version from a libstdc++ shared library
 * Examines the .gnu.version_d section to find the max version of the version strings of the form GLIBCXX_Major.Minor.Revision
 * Versions returned are of the form 0x00AABBCC
 * @return error_code_t
 */
STATIC error_code_t get_libstdcxx_version(const int fd, const char* const filename, uint32_t* const glibcxx_version) {
  ASSERT(fd >= 0, "Expecting an open file descriptor");
  ASSERT(filename && glibcxx_version, "Unexpected NULL arguments");

  struct stat st;
  if (fstat(fd, &st) < 0) {
    perror("Error getting file size");
    close(fd);
    return ec_fatal_error;
  }

  size_t file_size = (size_t)st.st_size;
  void* mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (mapped == MAP_FAILED) {
    perror("Error mapping file");
    close(fd);
    return ec_fatal_error;
  }

  close(fd);

  // Check ELF magic bytes
  ElfW(Ehdr)* ehdr = (ElfW(Ehdr)*)mapped;
  if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
    ERROR("File %s is not a valid ELF file\n", filename);
    munmap(mapped, file_size);
    return ec_fatal_error;
  }

  #if __ELF_NATIVE_CLASS == 64
    #define EXPECTED_ELFCLASS ELFCLASS64
  #elif __ELF_NATIVE_CLASS == 32
    #define EXPECTED_ELFCLASS ELFCLASS32
  #else
    #error "Unknown compilation target"
  #endif
  if (ehdr->e_ident[EI_CLASS] != EXPECTED_ELFCLASS) {
    // ELF does not match the expected class.
    // Parsing cannot continue as the offsets will not align
    // This is not a fatal error
    munmap(mapped, file_size);
    return ec_non_fatal_error;
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

        ASSERT((void*)verdaux < (mapped + file_size), "Invalid ELF, verdaux pointer is outside the ELF size\n");
        ASSERT(shdr[i].sh_offset < file_size, "Invalid ELF, verdef offset is outside the ELF size\n");
        char* string_table = (char*)mapped + shdr[shdr[i].sh_link].sh_offset;

        for (int vdaux_i = 0; vdaux_i < verdef->vd_cnt; vdaux_i++) {
          char* vdaux_name = (string_table + verdaux->vda_name);
          ASSERT((void*)vdaux_name < (mapped + file_size), "Invalid ELF, vdaux_name is outside the ELF size\n");
          if (0 == strncmp(vdaux_name, "GLIBCXX_", 8)) {
            const uint32_t version = version_string_to_int(vdaux_name + 8);
            if (!at_least_one_glibcxx_version_found) {
              *glibcxx_version = version;
              at_least_one_glibcxx_version_found = 1;
            } else {
              *glibcxx_version = uint_max(*glibcxx_version, version);
            }
            TRACE_ELF("Version %s i %x\n", vdaux_name, version);
            TRACE_ELF("*glibcxx_version = %x\n", *glibcxx_version);
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
    TRACE_ELF("No glibcxx version found\n");
    return ec_fatal_error;
  }
  return ec_success;
}

#endif
