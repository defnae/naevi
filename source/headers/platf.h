// headers/platf.h

#ifndef PLATF_H
#define PLATF_H

#ifdef __linux__
#define PLATFORM_LINUX 1
#elif defined(__APPLE__)
#define PLATFORM_DARWIN 1
#elif defined(__CYGWIN__) || defined(__MSYS__)
#define PLATFORM_MSYS 1
#else
#error "Unsupported platform. Supported platforms: Linux, macOS, Windows (MSYS2)."
#endif

#endif
