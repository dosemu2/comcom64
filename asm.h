#ifndef ASM_H
#define ASM_H

#ifndef __ASSEMBLER__
/* dummy out thunk_gen annotations for now */
#define __ASM(x, y) extern x y
#define __ASM_FUNC(x) void x(void)
#define ASMCFUNC
#define SEMIC ;

__ASM(unsigned short, ds) SEMIC
__ASM_FUNC(my_int23_handler) SEMIC

int ASMCFUNC do_int23(void);
int ASMCFUNC main(int argc, char *argv[], char *envp[]);
#endif

#endif
