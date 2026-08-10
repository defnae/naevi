// headers/abi.h

#ifndef ABI_H
#define ABI_H

#ifdef __x86_64__
#if defined(__CYGWIN__) || defined(__MSYS__) || defined(_WIN32)

#define ABI __attribute__((ms_abi))

#else

#define ABI __attribute__((sysvabi))
#endif
#else

#define ABI

#endif

#endif
