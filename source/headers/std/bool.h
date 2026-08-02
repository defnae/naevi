// headers/std/bool.h

#ifndef BOOL_H
#define BOOL_H

#ifndef __clang__
#error "Toolchain mismatch: Clang required."
#endif

typedef __UINT8_TYPE__ bool;

#define true ((bool) 1)
#define false ((bool) 0)

#endif
