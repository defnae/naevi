// source/headers/std/str.h

#ifndef STR_H
#define STR_H

/* ISO/IEC 9899:199409 (Unsafe) */

static char *strcpy(char *__restrict__, const char *__restrict__)
	__attribute__((diagnose_if(1, "`strcpy` is unsafe.", "warning")));

static __attribute__((always_inline)) __inline__ char *strcpy(x, y)
char *__restrict__ x;
const char *__restrict__ y;
{
	return __builtin_strcpy(x, y);
}

static char *strncpy(char *__restrict__, const char *__restrict__, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strncpy` is unsafe.", "warning")));

static __attribute__((always_inline)) __inline__ char *strncpy(x, y, z)
char *__restrict__ x;
const char *__restrict__ y;
__SIZE_TYPE__ z;
{
	return __builtin_strncpy(x, y, z);
}

static char *strcat(char *__restrict__, const char *__restrict__)
	__attribute__((diagnose_if(1, "`strcat` is unsafe; Use `strlcat` instead.", "warning")));

static __attribute__((always_inline)) __inline__ char *strcat(x, y)
char *__restrict__ x;
const char *__restrict__ y;
{
	return __builtin_strcat(x, y);
}

/* POSIX.1-2024 (Unsafe) */

static char *stpcpy(char *__restrict__, const char *__restrict__)
	__attribute__((diagnose_if(1, "`stpcpy` is unsafe; Use `stpncpy` instead.", "warning")));

static __attribute__((always_inline)) __inline__ char *stpcpy(x, y)
char *__restrict__ x;
const char *__restrict__ y;
{
	return __builtin_stpcpy(x, y);
}

/* ISO/IEC 9899:199409 */

#define memchr __builtin_memchr
#define memcmp __builtin_memcmp
#define memcpy __builtin_memcpy
#define memmove __builtin_memmove
#define memset __builtin_memset

/* #define strcat __builtin_strcat */

#define strchr __builtin_strchr
#define strcmp __builtin_strcmp

#ifdef WNONBUILTINS
int strcoll(const char *, const char *)
	__attribute__((diagnose_if(1, "`strcoll` is not a builtin.", "warning")));
#else
int strcoll(const char *, const char *);
#endif /* #define strcoll __builtin_strcoll */

/* #define strcpy __builtin_strcpy */

#define strcspn __builtin_strcspn

#ifdef WNONBUILTINS
char *strerror(int)
	__attribute__((diagnose_if(1, "`strerror` is not a builtin.", "warning")));
#else
char *strerror(int);
#endif /* #define strerror __builtin_strerror */

#define strlen __builtin_strlen
#define strncat __builtin_strncat
#define strncmp __builtin_strncmp

/* #define strncpy __builtin_strncpy */

#define strpbrk __builtin_strpbrk
#define strrchr __builtin_strrchr
#define strspn __builtin_strspn
#define strstr __builtin_strstr

#ifdef WNONBUILTINS
char *strtok(char *, const char *)
	__attribute__((diagnose_if(1, "`strtok` is not a builtin.", "warning")));
#else
char *strtok(char *, const char *);
#endif /* #define strtok __builtin_strtok */

#ifdef WNONBUILTINS
__SIZE_TYPE__ strxfrm(char *, const char *, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strxfrm` is not a builtin.", "warning")));
#else
__SIZE_TYPE__ strxfrm(char *, const char *, __SIZE_TYPE__);
#endif /* #define strxfrm __builtin_strxfrm */

/* POSIX.1-2024 */

#if !defined(_LOCALE_T_DEFINED) && !defined(__locale_t_defined) && !defined(_LOCALE_T)
typedef void *locale_t;
#endif

#ifdef WNONBUILTINS
int strcoll_l(const char *, const char *, locale_t)
	__attribute__((diagnose_if(1, "`strcoll_l` is not a builtin.", "warning")));
#else
int strcoll_l(const char *, const char *, locale_t);
#endif /* #define strcoll_l __builtin_strcoll_l */

#ifdef WNONBUILTINS
char *strerror_l(int, locale_t)
	__attribute__((diagnose_if(1, "`strerror_l` is not a builtin.", "warning")));
#else
char *strerror_l(int, locale_t);
#endif /* #define strerror_l __builtin_strerror_l */

#ifdef WNONBUILTINS
__SIZE_TYPE__ strxfrm_l(char *, const char *, __SIZE_TYPE__, locale_t)
	__attribute__((diagnose_if(1, "`strxfrm_l` is not a builtin.", "warning")));
#else
__SIZE_TYPE__ strxfrm_l(char *, const char *, __SIZE_TYPE__, locale_t);
#endif /* #define strxfrm_l __builtin_strxfrm_l */

#ifdef WNONBUILTINS
char *strtok_r(char *, const char *, char **)
	__attribute__((diagnose_if(1, "`strtok_r` is not a builtin.", "warning")));
#else
char *strtok_r(char *, const char *, char **);
#endif /* #define strtok_r __builtin_strtok_r */

#ifdef WNONBUILTINS
void *memccpy(void *, const void *, int, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`memccpy` is not a builtin.", "warning")));
#else
void *memccpy(void *, const void *, int, __SIZE_TYPE__);
#endif /* #define memccpy __builtin_memccpy */

/* #define stpcpy __builtin_stpcpy */

#define stpncpy __builtin_stpncpy
#define strdup __builtin_strdup
#define strndup __builtin_strndup

#ifdef WNONBUILTINS
int strerror_r(int, char *, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strerror_r` is not a builtin.", "warning")));
#else
int strerror_r(int, char *, __SIZE_TYPE__);
#endif /* #define strerror_r __builtin_strerror_r */

#ifdef WNONBUILTINS
__SIZE_TYPE__ strlcat(char *, const char *, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strlcat` is not a builtin.", "warning")));
#else
__SIZE_TYPE__ strlcat(char *, const char *, __SIZE_TYPE__);
#endif /* #define strlcat __builtin_strlcat */

#ifdef WNONBUILTINS
__SIZE_TYPE__ strlcpy(char *, const char *, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strlcpy` is not a builtin.", "warning")));
#else
__SIZE_TYPE__ strlcpy(char *, const char *, __SIZE_TYPE__);
#endif /* #define strlcpy __builtin_strlcpy */

#ifdef WNONBUILTINS
__SIZE_TYPE__ strnlen(const char *, __SIZE_TYPE__)
	__attribute__((diagnose_if(1, "`strnlen` is not a builtin.", "warning")));
#else
__SIZE_TYPE__ strnlen(const char *, __SIZE_TYPE__);
#endif /* #define strnlen __builtin_strnlen */

#ifdef WNONBUILTINS
char *strsignal(int)
	__attribute__((diagnose_if(1, "`strsignal` is not a builtin.", "warning")));
#else
char *strsignal(int);
#endif /* #define strsignal __builtin_strsignal */

#ifdef WNONBUILTINS
char *basename(char *)
	__attribute__((diagnose_if(1, "`basename` is not a builtin.", "warning")));
#else
char *basename(char *);
#endif /* #define basename __builtin_basename */

#endif /* STR_H */
