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
isr_stub 255

isr_common:
#Pushe alle Register
mov %rax,-0x8(%rsp)
mov %rbx,-0x10(%rsp)
mov %rcx,-0x18(%rsp)
mov %rdx,-0x20(%rsp)
mov %r8,-0x28(%rsp)
mov %r9,-0x30(%rsp)
mov %r10,-0x38(%rsp)
mov %r11,-0x40(%rsp)
mov %r12,-0x48(%rsp)
mov %r13,-0x50(%rsp)
mov %r14,-0x58(%rsp)
mov %r15,-0x60(%rsp)
#rsp nicht pushen, da die CPU das Register schon gepusht hat
mov %rbp,-0x68(%rsp)
mov %rdi,-0x70(%rsp)
mov %rsi,-0x78(%rsp)

#Pushe Selektorregister
#SS und CS müssen nicht gepusht werden, da diese schon von der CPU gepusht werden
mov %ds,%eax
mov %rax,-0x80(%rsp)
mov %es,%eax
mov %rax,-0x88(%rsp)
mov %fs,%eax
mov %rax,-0x90(%rsp)
mov %gs,%eax
mov %rax,-0x98(%rsp)

sub $0x98,%rsp

#Segmentregister laden
mov $0x10,%eax
mov %ax,%ds
mov %ax,%es
mov %ax,%fs
mov %ax,%gs

#Dies ist der Parameter für die Funktion isr_Handler
mov %rsp,%rdi
#Aufruf des Handlers
call isr_Handler
#Zurückgegebener Wert ist entweder ein veränderter oder unveränderten Stack Pointer
#+0x10 um den Fehlercode und Interruptnummer vom Stack zu nehmen
lea 0xa8(%rax),%rsp

#Und jetzt wieder alle Registerwerte herstellen. Und zwar in umgekehrter Reihenfolge. Wir verwenden rax als stackpointer damit wir keine dependency auf das lea haben
mov (%rax),%rdx
mov %dx,%gs
mov 0x8(%rax),%rdx
mov %dx,%fs
mov 0x10(%rax),%rdx
mov %dx,%es
mov 0x18(%rax),%rdx
mov %dx,%ds

mov -0x88(%rsp),%rsi
mov -0x80(%rsp),%rdi
mov -0x78(%rsp),%rbp
mov -0x70(%rsp),%r15
mov -0x68(%rsp),%r14
mov -0x60(%rsp),%r13
mov -0x58(%rsp),%r12
mov -0x50(%rsp),%r11
mov -0x48(%rsp),%r10
mov -0x40(%rsp),%r9
mov -0x38(%rsp),%r8
mov -0x30(%rsp),%rdx
mov -0x28(%rsp),%rcx
mov -0x20(%rsp),%rbx
mov -0x18(%rsp),%rax

#Rücksprung
iretq


.global isr_syscall
.extern syscall_syscallHandler
.extern tss
#Parameter:
#rdi = Funktion
#rsi = 1. Parameter
#rdx = 2. Parameter
#r10 = 3. Parameter
#r8  = 4. Parameter
#r9  = 5. Parameter
isr_syscall:
#rsp zwischenspeichern
mov %rsp,%rax
#Den Kernelstackpointer laden wir einfach aus der TSS
movq (tss+0x4),%rsp

#rip sichern
mov %rcx,-0x8(%rsp)
#rsp sichern
mov %rax,-0x10(%rsp)
#rflags sichern
mov %r11,-0x18(%rsp)

#Stack muss 16-byte aligned sein beim Funktionsaufruf
sub $0x20,%rsp

#3. Parameter an die richtige Position verschieben
mov %r10,%rcx

call syscall_syscallHandler

#rflags laden
mov 0x8(%rsp),%r11
#rsp zwischenladen
mov 0x10(%rsp),%r10
#rip laden
mov 0x18(%rsp),%rcx

#Interrupts deaktivieren, weil sonst kann das böse enden (Stack)
cli
#rsp laden
mov %r10,%rsp
sysretq
