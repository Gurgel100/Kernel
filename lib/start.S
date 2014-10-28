.ifndef BUILD_KERNEL
.code64

.extern initLib
.global _start
_start:
call initLib

.endif
