.ifndef BUILD_KERNEL
.code64

.extern initLib
.extern main
.global _start
_start:
push %rax
push %rbx
call initLib
pop %rsi
pop %rdi
call main
mov %rax,%rdi
call exit

.endif
