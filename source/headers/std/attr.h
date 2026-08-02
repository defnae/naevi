// headers/std/attr.h

#ifndef ATTR_H
#define ATTR_H

#ifndef __clang__
#error "Toolchain mismatch: Clang required."
#endif

#define MSABI __attribute__((ms_abi))
#define SYSVABI __attribute__((sysv_abi))

#define INLINE __attribute__((always_inline)) inline
#define NORETURN __attribute__((noreturn))

#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))

#define UNUSED __attribute__((unused))

#endif
