#include "asm.h"
.include "asm.inc"

.text
.global _my_int23_handler
_my_int23_handler:
    pusha
    handler_prolog SIGSTK_LEN
    call _do_int23
    restore_stack

    or %eax, %eax
    jnz 1f
    popa
    iret
1:
    popa
    stc
    lret
