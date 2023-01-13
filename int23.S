.bss
.global _ds
_ds: .long 0
_cstack: .fill 0x10000
_on_cstack: .long 0

.text
.global _my_int23_handler
_my_int23_handler:
    pusha
    push %ds
    mov %cs:_ds, %eax
    mov %eax, %ds
    xorl %eax, %eax
    cmpl _on_cstack, %eax
    jnz 2f
    incl _on_cstack
    mov %ss, %esi
    mov %esp, %edi
    pushl _ds
    lea _cstack+0x10000, %edx
    push %edx
    lss (%esp), %esp
    push %esi
    push %edi
    call _do_int23
    lss (%esp), %esp
    decl _on_cstack
5:
    pop %ds
    or %eax, %eax
    jnz 1f
    popa
    iret
1:
    popa
    stc
    lret
2:
    call _do_int23
    jmp 5b
