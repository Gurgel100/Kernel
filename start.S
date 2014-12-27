.ifdef BUILD_KERNEL
.code64
.extern main
.global _start

_start:
#Initialisiere Stack
mov $(stack),%rsp

#Stack mit 0xC füllen
mov %rsp,%rdi
mov $0x10000,%rcx
sub %rcx,%rdi
shr $3,%rcx
mov $0xCCCCCCCCCCCCCCCC,%rax
rep stosq

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
.space 0x10000		#64KB Platz für den Kernelstack
.global stack
stack:
.endif
