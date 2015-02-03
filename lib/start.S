.ifndef BUILD_KERNEL
.code64

.extern initLib
.extern main
.global _start
.global _syscall

_start:
push %rax
push %rbx

#im rcx Register werden flags übergeben
#Bit 0 gibt an, ob syscall unterstützt wird
test $1,%rcx
lea interrupt(%rip),%rax
lea syscall(%rip),%rdx
cmovne %rdx,%rax
mov %rax,_syscall(%rip)

call initLib
pop %rsi
pop %rdi
call main
mov %rax,%rdi
call exit

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
