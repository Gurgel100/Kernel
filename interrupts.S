.code64
.section .text
.extern isr_Handler
#Makro für allgemeinen Interrupt-Handler ohne Fehlercode
.macro isr_stub counter
.global int\counter
int\counter:
pushq $0
pushq $\counter
jmp isr_common
.endm

#Makro für allgemeinen Interrupt-Handler mit Fehlercode
.macro isr_stub_error counter
.global int\counter
int\counter:
pushq $\counter
jmp isr_common
.endm

#Exceptions
isr_stub 0
isr_stub 1
isr_stub 2
isr_stub 3
isr_stub 4
isr_stub 5
isr_stub 6
isr_stub 7
isr_stub_error 8
isr_stub 9
isr_stub_error 10
isr_stub_error 11
isr_stub_error 12
isr_stub_error 13
isr_stub_error 14
isr_stub 16
isr_stub_error 17
isr_stub 18
isr_stub 19

#IRQs
isr_stub 32
isr_stub 33
isr_stub 34
isr_stub 35
isr_stub 36
isr_stub 37
isr_stub 38
isr_stub 39
isr_stub 40
isr_stub 41
isr_stub 42
isr_stub 43
isr_stub 44
isr_stub 45
isr_stub 46
isr_stub 47

#Syscalls
isr_stub 48

isr_common:
#Pushe alle Register
push %rax
push %rbx
push %rcx
push %rdx
push %r8
push %r9
push %r10
push %r11
push %r12
push %r13
push %r14
push %r15
#rsp nicht pushen, da die CPU das Register schon gepusht hat
push %rbp
push %rdi
push %rsi

#Pushe Selektorregister
#SS und CS müssen nicht gepusht werden, da diese schon von der CPU gepusht werden
pushq $0
mov %ds,%rax
cltq
mov %rax,(%rsp)
pushq $0
mov %es,%rax
cltq
mov %rax,(%rsp)
pushq %fs
pushq %gs

#Segmentregister laden
mov $0x10,%ax
mov %ax,%ds
mov %ax,%es
mov %ax,%fs
mov %ax,%gs

#Dies ist der Parameter für die Funktion isr_Handler
mov %rsp,%rdi
#Aufruf des Handlers
call isr_Handler
#Zurückgegebener Wert ist entweder ein veränderter oder unveränderten Stack Pointer
mov %rax,%rsp

#Und jetzt wieder alle Registerwerte herstellen. Und zwar in umgekehrter Reihenfolge
popq %gs
popq %fs
mov (%rsp),%rax
mov %rax,%es
add $8,%rsp
mov (%rsp),%rax
mov %rax,%ds
add $8,%rsp

pop %rsi
pop %rdi
pop %rbp
pop %r15
pop %r14
pop %r13
pop %r12
pop %r11
pop %r10
pop %r9
pop %r8
pop %rdx
pop %rcx
pop %rbx
pop %rax

#Fehlercode und Interruptnummer vom Stack nehmen
add $16,%rsp

#Rücksprung
iretq
