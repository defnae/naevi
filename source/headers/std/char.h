// headers/std/char.h

#ifndef CHAR_H
#define CHAR_H

#ifndef __clang__
#error "Unsupported Compiler: Clang required."
#endif

#ifndef __CHAR_UNSIGNED__
#error "Signedness Mismatch: 'char' must be signed. Use '-funsigned-char.'"
#endif

#if __CHAR_BIT__ != 8
#error "Unsupported Architecture: 'char' isn't quite 8 bits."
#endif

typedef char char8_t;
typedef __INT8_TYPE__ schar8_t;

typedef __UINT16_TYPE__ char16_t;
typedef __INT16_TYPE__ schar16_t;

typedef __UINT32_TYPE__ char32_t;
typedef __INT32_TYPE__ schar32_t;

#endif
