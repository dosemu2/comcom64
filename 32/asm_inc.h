#define __ASM(x, y) extern x y
#define __ASM_FUNC(x) void x(void)
#define SEMIC ;
#include "glob_asm.h"
#undef __ASM
#undef __ASM_FUNC
#undef SEMIC
