#ifndef ASM_H
#define ASM_H

#ifndef __ASSEMBLER__

/* dummy out thunk_gen annotations for now */
#define __ASM(x, y) extern x y
#define __ASM_FUNC(x) void x(void)
#define ASMCFUNC
#define SEMIC ;

__ASM(unsigned short, _ds) SEMIC
__ASM(unsigned int, _prev0_eip) SEMIC
__ASM(unsigned short, _prev0_cs) SEMIC
__ASM_FUNC(my_int23_handler) SEMIC
__ASM_FUNC(my_int0_handler) SEMIC

int ASMCFUNC do_int23(void);
void ASMCFUNC do_int0(void);
int ASMCFUNC main(int argc, const char *argv[], const char *envp[]);

#else

#define SIGSTK_LEN 0x200

#endif

#endif