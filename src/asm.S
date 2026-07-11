.bss
.global __ds
__ds: .long 0

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
