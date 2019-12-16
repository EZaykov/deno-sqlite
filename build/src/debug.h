#ifndef DEBUG_H
#define DEBUG_H
#ifdef DEBUG_BUILD

#include <stdlib.h>
#include "sqlite3.h"
#include "buffer.h"

extern buffer* debug_log;

// TODO: Handle memory errors better ...
#define debug_printf(...) do { \
                            char* prefix = sqlite3_mprintf("DEBUG: %s:%d:%s(): ", __FILE__, __LINE__, __func__); \
                            char* msg    = sqlite3_mprintf(__VA_ARGS__); \
                            int len_p; \
                            int len_m; \
                            for (len_p = 0; prefix[len_p] != '\0'; len_p ++); \
                            for (len_m = 0; prefix[len_m] != '\0'; len_m ++); \
                            if (debug_log == NULL) \
                              debug_log = new_buffer(); \
                            if (debug_log == NULL) \
                              break; \
                            write_buffer(debug_log, prefix, debug_log->size, len_p); \
                            write_buffer(debug_log, msg, debug_log->size, len_m); \
                            sqlite3_free(prefix); \
                            sqlite3_free(msg); \
                          } while (0);

#else
#define debug_printf(...)
#endif // DEBUG_BUILD
#endif // DEBUG_H
