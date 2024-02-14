#ifndef ASM_H
#define ASM_H

#ifndef __ASSEMBLER__
/* dummy out fdpp tricks for now */
#define __ASMSYM(t) t*
#define __ASMREF(t) &t
#define __ASMFSYM(t) t**
#define __ASYM(x) *(x)
#include <dj64/glob_inc.h>

#define ASMCFUNC

int ASMCFUNC do_int23(void);
void ASMCFUNC do_int0(void);
int ASMCFUNC main(int argc, const char *argv[], const char *envp[]);

#else

#define SIGSTK_LEN 0x200

#endif

#endif
