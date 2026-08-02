// main.c

#include <headers/std/int.h>
#include <headers/std/char.h>
#include <headers/std/bool.h>
#include <headers/std/def.h>
#include <headers/std/arg.h>
#include <headers/std/str.h>
#include <headers/std/attr.h>

extern int puts(const char* __restrict__);

#ifdef _WIN32

#define ABI MSABI

#else

#define ABI SYSVABI

#endif

ABI int main(void) {
    puts("\nHello, world!");

    return 0;
}
