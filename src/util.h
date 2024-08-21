#include <stdio.h>

#define MESSAGE(type, message)                                                 \
  fprintf(stderr, "[%s] %s: %s\n", type, __func__, message)
#define WARNING(message) MESSAGE("WARN", message)
#define ERROR(message) MESSAGE("ERR", message)

// handle exception (assert but with action)
#define HANDLE(function, message, action)                                      \
  {                                                                            \
    if (!(function)) {                                                         \
      ERROR(message);                                                          \
      action;                                                                  \
    }                                                                          \
  }