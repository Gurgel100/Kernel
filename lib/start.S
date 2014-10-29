.ifndef BUILD_KERNEL
.code64

.extern initLib
.extern main
.global _start
_start:
call initLib
call main

.endif
