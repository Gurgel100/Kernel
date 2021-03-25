.ifndef BUILD_KERNEL
.code64

.extern c_main
.global _start

.section .text
_start:
mov %rsp,%rdi
#Stack wieder alignen
and $~0xF,%rsp

call c_main
#Hier sollten wir nie hinkommen
ud2

.global syscall
syscall:
mov %rcx,%r10
syscall
retq

.endif
