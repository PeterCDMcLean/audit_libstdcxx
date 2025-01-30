#ifndef _ERROR_TYPES_H_
#define _ERROR_TYPES_H_

typedef enum {
  ec_fatal_error     = -1,
  ec_success         = 0,
  ec_non_fatal_error = 1
} error_code_t;

#endif
