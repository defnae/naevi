#ifndef ARG_H
#define ARG_H

#ifndef __clang__
#error "Unsupported Compiler. Use Clang."
#endif

typedef __builtin_va_list va_list;

#define va_start(x, y) __builtin_va_start(x, y)
#define va_arg(x, y) __builtin_va_arg(x, y)
#define va_end(x) __builtin_va_end(x)

#endif
