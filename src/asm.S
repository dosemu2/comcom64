#include "asm.h"

.bss
.global __ds
__ds: .long 0
.ifndef _DJ64
/* We need emergency stack because __dpmi_int() alters SS in djgpp.
 * That was eventually fixed in dj64, hence the ifdef. */
.global _cstack
_cstack: .fill SIGSTK_LEN
.endif

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
