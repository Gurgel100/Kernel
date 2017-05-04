.ifndef BUILD_KERNEL
.code64

.extern c_main
.global _start
.global _syscall

.section .text
_start:
#Im rdi Register werden flags übergeben
#Bit 0 gibt an, ob syscall unterstützt wird
test $1,%rdi
lea interrupt(%rip),%rax
lea syscall(%rip),%rdx
cmovne %rdx,%rax
mov %rax,_syscall(%rip)

mov %rsp,%rdi
#Stack wieder alignen
and $~0xF,%rsp

call c_main
#Hier sollten wir nie hinkommen
ud2

interrupt:
int $0x30
retq

syscall:
mov %rcx,%r10
syscall
retq

.section .bss
_syscall: .quad 0

.endif
