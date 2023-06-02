section .text

extern dj64_plt_call

%macro asmcfunc_n 2
    global _%1
    _%1:
    push ebp
    mov ebp, esp
    pushad
    mov ecx, %2
    mov edx, esp
    call dj64_plt_call
    popad
    pop ebp
    ret
%endmacro

%include "plt.inc"

%macro asmsym 1
    extern _%1
    dd _%1
%endmacro

#define __ASM(t, n) asmsym n
#define __ASM_FAR(t, n) asmsym n
#define __ASM_NEAR(t, n) asmsym n
#define __ASM_ARR(t, n, l) asmsym n
#define __ASM_ARRI(t, n) asmsym n
#define __ASM_ARRI_F(t, n) asmsym n
#define __ASM_FUNC(n) asmsym n
#define SEMIC
__asm_symtab_start:
#include <glob_asm.h>
__asm_symtab_end:

#ifdef __ELF__
section .note.GNU-stack
#endif
