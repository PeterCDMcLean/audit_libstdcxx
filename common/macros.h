#ifndef _MACROS_H_
#define _MACROS_H_

/**
 *  These helper macros wrap fprintf and abort.
 *  All of them print to stderr as stdout appears to be less reliable in the audit library
 */

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

#endif
