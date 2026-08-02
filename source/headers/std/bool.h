// headers/std/bool.h

#ifndef BOOL_H
#define BOOL_H

#ifndef __clang__
#error "Unsupported Compiler. Use Clang."
#endif

typedef __UINT8_TYPE__ bool;

#define true ((bool) 1)
#define false ((bool) 0)

#endif
