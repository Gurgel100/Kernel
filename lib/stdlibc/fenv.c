#ifndef BUILD_KERNEL
#include "fenv.h"
#include <stdint.h>
#include <stdbool.h>
#include <immintrin.h>
#include "assert.h"

union fpu_control_word {
	uint16_t raw;
	struct {
		bool invalid_op             : 1;
		bool denormal               : 1;
		bool zero_div               : 1;
		bool overflow               : 1;
		bool underflow              : 1;
		bool precision              : 1;
		uint8_t res1                : 2;
		uint8_t precision_control   : 2;
		uint8_t rounding_control    : 2;
		bool infinity_control       : 1;
	};
};

union fpu_status_word {
	uint16_t raw;
	struct {
		bool invalid_operation	: 1;
		bool denormal_operand	: 1;
		bool divide_by_zero		: 1;
		bool overflow			: 1;
		bool underflow			: 1;
		bool precision			: 1;
		bool stack_fault		: 1;
		bool exc_summary_status	: 1;
	};
};

union mxcsr {
	uint32_t raw;
	struct {
		bool invalid_operation	: 1;
		bool denormal_operand	: 1;
		bool divide_by_zero		: 1;
		bool overflow			: 1;
		bool underflow			: 1;
		bool precision			: 1;
		bool denormal_are_zero	: 1;
		bool invalid_op_mask    : 1;
		bool denormal_op_mask   : 1;
		bool divide_by_zero_mask: 1;
		bool overflow_mask      : 1;
		bool underflow_mask     : 1;
		bool precision_mask     : 1;
		uint8_t rounding_control: 1;
		bool flush_to_zero      : 1;
	};
};

static_assert(_MM_EXCEPT_INVALID == FE_INVALID, "FE_INVALID does not correspond to correct bit in mxcsr");
static_assert(_MM_EXCEPT_DIV_ZERO == FE_DIVBYZERO, "FE_DIVBYZERO does not correspond to correct bit in mxcsr");
static_assert(_MM_EXCEPT_OVERFLOW == FE_OVERFLOW, "FE_OVERFLOW does not correspond to correct bit in mxcsr");
static_assert(_MM_EXCEPT_UNDERFLOW == FE_UNDERFLOW, "FE_UNDERFLOW does not correspond to correct bit in mxcsr");
static_assert(_MM_EXCEPT_INEXACT == FE_INEXACT, "FE_INEXACT does not correspond to correct bit in mxcsr");

int feclearexcept(int excepts) {
	// Reset FPU exceptions
	if (excepts == (FE_ALL_EXCEPT)) {
		asm volatile("fclex");
	} else {
		fenv_t env;
		asm("fnstenv %0": "=m"(env.fpu_env));
		env.fpu_env.status_word &= ~excepts;
		asm volatile("fldenv %0": : "m"(env.fpu_env));
	}

	// Reset mxcsr
	_MM_SET_EXCEPTION_STATE(_MM_GET_EXCEPTION_STATE() & ~excepts);

	return 0;
}

int fetestexcept(int excepts) {
	uint16_t fpu_status;
	asm("fnstsw %0": "=am"(fpu_status));

	return excepts & ((fpu_status & _MM_EXCEPT_MASK) | _MM_GET_EXCEPTION_STATE());
}

int feraiseexcept(int excepts) {
	if (excepts & FE_INVALID) {
		asm volatile("divss %0, %0":: "x"(0));
	}
	if (excepts & FE_DIVBYZERO) {
		asm volatile("divss %0, %1":: "x"(0), "x"(1));
	}
	if (excepts & FE_UNDERFLOW) {
		fenv_t env;
		asm("fnstenv %0": "=m"(env.fpu_env));
		env.fpu_env.status_word |= FE_UNDERFLOW;
		asm volatile("fldenv %0;fwait": : "m"(env.fpu_env));
	}
	if (excepts & FE_OVERFLOW) {
		fenv_t env;
		asm("fnstenv %0": "=m"(env.fpu_env));
		env.fpu_env.status_word |= FE_OVERFLOW;
		asm volatile("fldenv %0;fwait": : "m"(env.fpu_env));
	}
	if (excepts & FE_INEXACT) {
		fenv_t env;
		asm("fnstenv %0": "=m"(env.fpu_env));
		env.fpu_env.status_word |= FE_INEXACT;
		asm volatile("fldenv %0;fwait": : "m"(env.fpu_env));
	}
	return 0;
}

int fegetexceptflag(fexcept_t* flagp, int excepts) {
	asm("fnstsw %0": "=am"(*flagp));
	*flagp |= _MM_GET_EXCEPTION_STATE();
	*flagp &= excepts & _MM_EXCEPT_MASK;
	return 0;
}

int fesetexceptflag(const fexcept_t* flagp, int excepts) {
	uint16_t flags = *flagp & excepts;
	// Since there is no fldsw instruction we need to take a detour
	fenv_t env;
	asm("fnstenv %0": "=m"(env.fpu_env));
	env.fpu_env.status_word = (env.fpu_env.status_word & ~_MM_EXCEPT_MASK) | flags;
	asm volatile("fldenv %0": : "m"(env.fpu_env));
	_MM_SET_EXCEPTION_STATE(flags);
	return 0;
}

int fesetround(int round) {
	round &= 0b11;
	uint16_t control_word;
	asm("fnstcw %0": "=m"(control_word));
	control_word |= round << 10;
	asm volatile("fldcw %0":: "m"(control_word));
	_MM_SET_ROUNDING_MODE(round << 13);
	return 0;
}

int fegetround() {
	// We only get the rounding mode of the sse environment
	// because the x87 environment should be in the same state
	return _MM_GET_ROUNDING_MODE() >> 13;
}

int fegetenv(fenv_t *envp) {
	asm("fnstenv %0": "=m"(envp->fpu_env));
	envp->mxcsr = _mm_getcsr();
	return 0;
}

int fesetenv(const fenv_t *envp) {
	asm volatile("fldenv %0": : "m"(envp->fpu_env));
	_mm_setcsr(envp->mxcsr);
	return 0;
}

int feholdexcept(fenv_t* envp) {
	int r = fegetenv(envp);
	if (r) return r;

	fenv_t new_env = *envp;
	// Do not raise any exception
	new_env.fpu_env.control_word &= ~_MM_EXCEPT_MASK;
	// Clear all status flags
	new_env.fpu_env.status_word &= ~_MM_EXCEPT_MASK;

	new_env.mxcsr &= ~_MM_EXCEPT_MASK;
	new_env.mxcsr |= _MM_MASK_MASK;

	return fesetenv(&new_env);
}

int feupdateenv(const fenv_t *envp) {
	fexcept_t exceptions;
	int r;
	if ((r = fegetexceptflag(&exceptions, FE_ALL_EXCEPT))) return r;
	if ((r = fesetenv(envp))) return r;
	return feraiseexcept(exceptions);
}
#endif /* BUILD_KERNEL */
