#include "asm.h"

.bss
.global __ds
__ds: .long 0
.global _cstack
_cstack: .fill SIGSTK_LEN
