%macro asmcfunc 3
;    global _%1
;    _%1:
;    mov al, 0 ; reserved
;    mov bh, 0 ; reserved
;    mov bl, DL_CCALL
;    mov cx, %2
;    mov dx, sp
;    add dx, %3
;    mov si, cs
;    call_plt
%endmacro

%macro asmcfunc_f 2
    asmcfunc %1, %2, 4
    retf
%endmacro

%macro asmcfunc_n 2
    asmcfunc %1, %2, 2
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
