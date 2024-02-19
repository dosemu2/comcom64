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

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
