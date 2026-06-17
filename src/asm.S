#include "asm.h"

.bss
.global __ds
__ds: .long 0
/* We need emergency stack because __dpmi_int() alters SS in djgpp.
 * That was eventually fixed in dj64, but gcc still does esp-relative
 * addressing. */
.global _cstack
_cstack: .fill SIGSTK_LEN

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
