#ifdef DEBUG_BUILD

#include <stdlib.h>
#include "buffer.h"

buffer* debug_log = NULL;

// Return buffer to JS
buffer* __attribute__((used)) __attribute__ ((visibility ("default"))) get_debug_buffer() {
  return debug_log;
}

#endif
