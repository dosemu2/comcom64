#include "asm.h"
.include "asm.inc"

.bss
.balign 2
__prev0:
.global __prev0_eip
__prev0_eip: .long 0
.global __prev0_cs
__prev0_cs: .word 0

.text
.global _my_int0_handler
_my_int0_handler:
    pusha
    handler_prolog SIGSTK_LEN
    call _do_int0
    restore_stack
    popa
    ljmpl *__prev0
