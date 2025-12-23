#ifndef ASM_H
#define ASM_H

#ifndef __ASSEMBLER__
#ifdef DJ64
#include <dj64/asm_inc.h>
#else
#include "asm_inc.h"
#endif

#define ASMCFUNC

int ASMCFUNC do_int23(void);
void ASMCFUNC do_int0(void);
int ASMCFUNC do_int75(void);
void ASMCFUNC do_mouse(void);

#else

#define SIGSTK_LEN 0x200

#endif

#endif
