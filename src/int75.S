/*
 *  comcom64 - 64bit command.com
 *  Copyright (C) 2023-2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "asm.h"
.include "asm.inc"

.bss
.balign 2
__prev75:
.global __prev75_eip
__prev75_eip: .long 0
.global __prev75_cs
__prev75_cs: .word 0

.text
.global _my_int75_handler
_my_int75_handler:
    pusha
    handler_prolog SIGSTK_LEN
    call _do_int75
    restore_stack
    testl %eax, %eax
    jz 0f
    fnclex
0:
    popa
    ljmpl *__prev75

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
