.code64

.global setjmp
.global longjmp

.section .text
setjmp:
	mov (%rsp), %rax
	lea -8(%rsp), %rcx
	mov %rcx, 0x0(%rdi)
	mov %rax, 0x8(%rdi)
	mov %rbx, 0x10(%rdi)
	mov %rbp, 0x18(%rdi)
	mov %r12, 0x20(%rdi)
	mov %r13, 0x28(%rdi)
	mov %r14, 0x30(%rdi)
	mov %r15, 0x38(%rdi)
	xor %eax, %eax
	ret

longjmp:
	//Restore all state
	mov 0x0(%rdi), %rsp
	mov 0x10(%rdi), %rbx
	mov 0x18(%rdi), %rbp
	mov 0x20(%rdi), %r12
	mov 0x28(%rdi), %r13
	mov 0x30(%rdi), %r14
	mov 0x38(%rdi), %r15
	mov $1, %eax
	test %esi, %esi
	cmovz %eax, %esi
	mov %esi, %eax
	jmp *0x8(%rdi)
