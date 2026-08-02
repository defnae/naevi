// headers/mem.h

#ifndef MEM_H
#define MEM_H

#ifndef __clang__
#error "Toolchain mismatch: Clang required."
#endif

#include <headers/std/def.h>

/* ISO C89 allocation functions, declared by hand instead of
 * pulling in <stdlib.h>. these three symbols are guaranteed by
 * the C standard itself, not by any OS, so this is safe on
 * every target we support. */

extern void *malloc(size_t size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);

#endif
