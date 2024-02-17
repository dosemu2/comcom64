#ifndef ASM_H
#define ASM_H

#ifndef __ASSEMBLER__
#include <dj64/glob_inc.h>

#define ASMCFUNC

int ASMCFUNC do_int23(void);
void ASMCFUNC do_int0(void);

#else

#define SIGSTK_LEN 0x200

#endif

#endif
