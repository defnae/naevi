// headers/std/str.h

#ifndef STR_H
#define STR_H

#define memcpy __builtin_memcpy
#define memmove __builtin_memmove
#define memccpy __builtin_memccpy
#define memset __builtin_memset
#define memcmp __builtin_memcmp

#define memchr __builtin_memchr

#define strcpy __builtin_strcpy
#define strncpy __builtin_strncpy
#define strcat __builtin_strcat
#define strncat __builtin_strncat

#define strcmp __builtin_strcmp
#define strncmp __builtin_strncmp

#define strxfrm __builtin_strxfrm

#define strchr __builtin_strchr
#define strrchr __builtin_strrchr

#define strcspn __builtin_strcspn
#define strspn __builtin_strspn

#define strpbrk __builtin_strpbrk
#define strstr __builtin_strstr

#define strtok __builtin_strtok

#define strlen __builtin_strlen

#define strerror __builtin_strerror

#define stpcpy __builtin_stpcpy
#define stpncpy __builtin_stpncpy

#endif
