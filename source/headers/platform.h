// headers/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

#ifndef __clang__
#error "Toolchain mismatch: Clang required."
#endif

#if defined(__linux__)
#define NAEVI_LINUX 1
#elif defined(__APPLE__)
#define NAEVI_DARWIN 1
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define NAEVI_MSYS 1
#else
#error "Unsupported platform: only Linux, macOS, and MSYS2 are supported."
#endif

#endif
