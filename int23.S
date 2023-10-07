#include "asm.h"

.text
.global _my_int23_handler
_my_int23_handler:
    pusha
    push %ds
    mov %cs:__ds, %eax
    mov %eax, %ds
    mov %ss, %esi
    mov %esp, %edi
    pushl __ds
    lea _cstack+SIGSTK_LEN, %edx
    push %edx
    lss (%esp), %esp
    push %esi
    push %edi
    call _do_int23
    lss (%esp), %esp
    pop %ds

    or %eax, %eax
    jnz 1f
    popa
    iret
1:
    popa
    stc
    lret
