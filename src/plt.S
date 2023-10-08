.text

.macro asmcfunc_n nm,num
    .global _\nm
    _\nm:
    push %ebp
    mov %esp, %ebp
    pushal
    mov $\num, %ecx
    mov %esp, %edx
    call dj64_plt_call
    popal
    pop %ebp
    ret
.endm

#include "plt.inc"

.macro asmsym nm
    .long _\nm
.endm

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
.section .note.GNU-stack,"",%progbits
#endif
