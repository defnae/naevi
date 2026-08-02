// headers/std/def.h

#ifndef DEF_H
#define DEF_H

#ifndef __clang__
#error "Unsupported Compiler. Use Clang."
#endif

#define NULL ((void*) 0)

#define offsetof(type, member) __builtin_offsetof(type, member)

typedef __SIZE_TYPE__ size_t;

typedef __PTRDIFF_TYPE__ ptrdiff_t;

#endif
