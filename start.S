#ifdef BUILD_KERNEL
.code64
.extern main
.global _start

_start:
#Initialisiere Stack
mov $(stack),%rsp

#Lege Multiboot-Struktur-Adresse auf den Stack
#push %rbx
#Multiboot-Struktur_Adresse in edi übergeben, weil der gcc das so verlangt laut disassembler
mov %rbx,%rdi
call main
_stop:
cli
hlt
jmp _stop

.section .rodata
.space 0x100000		#1MB Platz für den Kernelstack
stack:
#endif
