.code64
.extern main
.global _start
.global stack

_start:
#Initialisiere Stack
mov $(stack),%rsp
xor %ebp, %ebp

#Lege Multiboot-Struktur-Adresse auf den Stack
#push %rbx
#Multiboot-Struktur_Adresse in edi übergeben, weil der gcc das so verlangt laut disassembler
mov %rbx,%rdi
call main
_stop:
cli
hlt
jmp _stop

.section .bss
.align 0x10
.space 0x10000		#64KB Platz für den Kernelstack
stack:
