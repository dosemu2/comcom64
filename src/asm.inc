.macro handler_prolog len
    push %ds
    mov %cs:__ds, %eax
    mov %eax, %ds
    mov %ss, %esi
    mov %esp, %edi
    pushl __ds
    lea _cstack+\len, %edx
    push %edx
    lss (%esp), %esp
    push %esi
    push %edi
.endm

.macro restore_stack
    lss (%esp), %esp
    pop %ds
.endm
