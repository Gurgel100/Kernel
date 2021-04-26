/*
 * math.c
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#include "math.h"
#include <stdint.h>
#include <immintrin.h>
#include <stdbool.h>

#define _CONCAT(x, y)		x##y
#define CONCAT(x, y)		_CONCAT(x, y)
#define OP_SUFFIX(op, x)	_Generic((x),					\
								float: CONCAT(op, _ps),		\
								double: CONCAT(op, _pd)		\
							)
#define TO_SSE(x)			_Generic((x),					\
								float: _mm_set_ss,			\
								double: _mm_set_sd			\
							)(x)
#define FROM_SSE(x)			_Generic((x),					\
								__m128: _mm_cvtss_f32,		\
								__m128d: _mm_cvtsd_f64		\
							)(x)

#define UNARYOP(op, x)			FROM_SSE(OP_SUFFIX(CONCAT(_mm_, op), x)(TO_SSE(x)))
#define UNARYOPA(op, x, ...)	FROM_SSE(OP_SUFFIX(CONCAT(_mm_, op), x)(TO_SSE(x), __VA_ARGS__))
#define BINOP(op, x, y)			FROM_SSE(OP_SUFFIX(CONCAT(_mm_, op), x)(TO_SSE(x), TO_SSE(y)))

#define DEFINE_UNARYOP(name, type, op)	\
	type name(type x) {					\
		return UNARYOP(op, x);			\
	}

#define DEFINE_BINOP(name, type, op)	\
	type name(type x, type y) {			\
		return BINOP(op, x, y);			\
	}

typedef union {
	struct {
		uint32_t mantissa: FLT_MANT_DIG - 1;
		uint32_t exponent: sizeof(float) * CHAR_BIT - (FLT_MANT_DIG - 1) - 1;
		bool sign: 1;
	};
	uint32_t i;
	float f;
} float_struct_t;

typedef union {
	struct {
		uint64_t mantissa: DBL_MANT_DIG - 1;
		uint32_t exponent: sizeof(double) * CHAR_BIT - (DBL_MANT_DIG - 1) - 1;
		bool sign: 1;
	};
	uint64_t i;
	double f;
} double_struct_t;

typedef union {
	struct {
		uint64_t mantissa: LDBL_MANT_DIG - 1;
		bool : 1;
		uint32_t exponent: 80 - (LDBL_MANT_DIG - 1) - 2;
		bool sign: 1;
	};
	uint128_t i;
	long double f;
} long_double_struct_t;

// Basic operations

float fabsf(float x) {
	return BINOP(andnot, -0.0f, x);
}

double fabs(double x) {
	return BINOP(andnot, -0.0, x);
}

long double fabsl(long double x) {
	union {
		long double f;
		uint128_t i;
	} tmp;
	tmp.f = x;
	tmp.i &= ~((uint128_t)1 << (sizeof(long double) * CHAR_BIT - 1));
	return tmp.f;
}

float fmodf(float x, float y) {
	if (y == 0) return NAN;
	if (x == 0 || (isinf(y) && !isinf(x))) return x;
	return x - truncf(x / y) * y;
}

double fmod(double x, double y) {
	if (y == 0) return NAN;
	if (x == 0 || (isinf(y) && !isinf(x))) return x;
	return x - trunc(x / y) * y;
}

long double fmodl(long double x, long double y) {
	if (isinf(x) || y == 0) return NAN;
	bool repeat;
	do {
		asm volatile("fprem;"
			"fnstsw %%ax;"
			"sahf;"
			: "+t"(x), "=@ccp"(repeat): "u"(y): "ax", "cc");
	} while(repeat);
	return x;
}

float remainderf(float x, float y) {
	if (y == 0) return NAN;
	if (x == 0 || (isinf(y) && !isinf(x))) return x;
	return x - roundevenf(x / y) * y;
}

double remainder(double x, double y) {
	if (y == 0) return NAN;
	if (x == 0 || (isinf(y) && !isinf(x))) return x;
	return x - roundeven(x / y) * y;
}

long double remainderl(long double x, long double y) {
	if (isinf(x) || y == 0) return NAN;
	bool repeat;
	do {
		asm volatile("fprem1;"
			"fnstsw %%ax;"
			"sahf;"
			: "+t"(x), "=@ccp"(repeat): "u"(y): "ax", "cc");
	} while(repeat);
	return x;
}

DEFINE_BINOP(fmaxf, float, max)
DEFINE_BINOP(fmax, double, max)

long double fmaxl(long double x, long double y) {
	return x > y ? x : y;
}

DEFINE_BINOP(fminf, float, min)
DEFINE_BINOP(fmin, double, min)

long double fminl(long double x, long double y) {
	return x < y ? x : y;
}

float fdimf(float x, float y) {
	if (isnan(x) || isnan(y)) return NAN;
	return fmaxf(x - y, 0);
}

double fdim(double x, double y) {
	if (isnan(x) || isnan(y)) return NAN;
	return fmax(x - y, 0);
}

long double fdiml(long double x, long double y) {
	if (isnan(x) || isnan(y)) return NAN;
	return fmaxl(x - y, 0);
}

float nanf(const char *arg) {
	return __builtin_nanf(arg);
}

double nan(const char *arg) {
	return __builtin_nan(arg);
}

long double nanl(const char *arg) {
	return __builtin_nanl(arg);
}

// ----- Exponential functions -----

float expf(float x) {
	return expl(x);
}

double exp(double x) {
	return expl(x);
}

long double expl(long double x) {
    if (isinf(x)) return signbit(x) ? 0 : x;
	// The basic idea is to convert e^x to 2^x
	long double log2ex;
	asm volatile(
		"fldl2e;"
		// st0 = log2(e), st1 = x
		"fmulp;"
		// st0 = x * log2(e)
		:"=t"(log2ex) :"0"(x)
	);
	return exp2l(log2ex);
}

long double exp2l(long double x) {
    if (isinf(x)) return signbit(x) ? 0 : x;
    // Because f2xm1 can only exponate with -1 <= x <= 1
    // we have to split x into an integer part and an float part
	long double res;
	asm volatile(
		"frndint;"
		// st0 = int(x), st1 = x
		"fld1;"
		// st0 = 1, st1 = int(x), st2 = x
		"fscale;"
		// st0 = 2 ** int(x), st1 = int(x), st2 = x
		"fxch %%st(2);"
		// st0 = x, st1 = int(x), st2 = 2 ** int(x)
		"fsubp %%st(1);"
		// st0 = x - int(x), st1 = 2 ** int(x)
		"f2xm1;"
		// st0 = 2 ** (x - int(x)) - 1, st1 = 2 ** int(x)
		"fld1;"
		// st0 = 1, st1 = 2 ** (x - int(x)) - 1, st2 = 2 ** int(x)
		"faddp;"
		// st0 = 2 ** (x - int(x)), st1 = 2 ** int(x)
		"fmulp;"
		// st0 = 2 ** (x)
		:"=t"(res): "0"(x), "u"(x)
	);
	return res;
}

// Natural logarithm (base e)
float logf(float x) {
	return logl(x);
}

double log(double x) {
	return logl(x);
}

long double logl(long double x) {
    if (x < 0) return NAN;
    if (x == 0) return -INFINITY;
    if (x == 1) return 0;
	// ln(x) = log2(x) / log2(e)
	long double res;
	asm(
		"fyl2x;"
		"fldl2e;"
	    "fdivrp"
		: "=t"(res): "0"(x), "u"(1.0): "st(1)"
	);
	return res;
}

// Common logarithm (base 10)
float log10f(float x) {
	return log10l(x);
}

double log10(double x) {
	return log10l(x);
}

long double log10l(long double x) {
    if (x < 0) return NAN;
    if (x == 0) return -INFINITY;
    if (x == 1) return 0;
	// log10(x) = log2(x) / log2(10)
	long double res;
	asm(
        "fyl2x;"
        "fldl2t;"
	    "fdivrp"
        : "=t"(res): "0"(x), "u"(1.0): "st(1)"
	);
	return res;
}

// Binary logarithm (base 2)
float log2f(float x) {
	return log2l(x);
}

double log2(double x) {
	return log2l(x);
}

long double log2l(long double x) {
    if (x < 0) return NAN;
    if (x == 0) return -INFINITY;
	long double res;
	asm("fyl2x": "=t"(res): "0"(x), "u"(1.0): "st(1)");
	return res;
}

// ----- Power functions -----

// TODO: implement pow without log (to remove x > 0 constraint)
float powf(float x, float y) {
	return expf(y * logf(x));
}

double pow(double x, double y) {
	return exp(y * log(x));
}

long double powl(long double x, long double y) {
	return expl(y * logl(x));
}

DEFINE_UNARYOP(sqrtf, float, sqrt)
DEFINE_UNARYOP(sqrt, double, sqrt)

long double sqrtl(long double x) {
	if (x < 0) return NAN;
	long double res;
	asm("fsqrt": "=t"(res): "0"(x));
	return res;
}

float cbrtf(float x) {
	// Argument reduction
	int exp;
	float fr = frexpf(x, &exp);
	int shx = exp % 3;
	if (shx > 0) {
		// Compute shx such that (exp - shx) is divisible by 3
		shx -= 3;
	}
	exp = (exp - shx) / 3; // exponent of cube root
	fr = ldexpf(fr, shx);
	// 0.125 <= fr < 1.0

	// Compute seed with a quadratic approximation
	fr = (-0.46946116f * fr + 1.072302f) * fr + 0.3812513f;	// 0.5 <= fr < 1
	float r = ldexpf(fr, exp);	// 6 bits of precision

	// Use Newton-Raphson iterations to improve precision
	// Each iterations doubles the precision (e.g. after 1. iterations we have 12 bits, after 2. we have 24 and so forth)
	// For float we need 2 iterations (23 bit mantissa)
	for (int i = 0; i < 2; i++) {
		r = (2.f / 3.f) * r + (1.f / 3.f) * x / (r * r);
	}

	return r;
}

double cbrt(double x) {
	// Argument reduction
	int exp;
	double fr = frexp(x, &exp);
	int shx = exp % 3;
	if (shx > 0) {
		// Compute shx such that (exp - shx) is divisible by 3
		shx -= 3;
	}
	exp = (exp - shx) / 3; // exponent of cube root
	fr = ldexp(fr, shx);
	// 0.125 <= fr < 1.0

	// Compute seed with a quadratic approximation
	fr = (-0.46946116 * fr + 1.072302) * fr + 0.3812513;	// 0.5 <= fr < 1
	double r = ldexp(fr, exp);	// 6 bits of precision

	// Use Newton-Raphson iterations to improve precision
	// Each iterations doubles the precision (e.g. after 1. iterations we have 12 bits, after 2. we have 24 and so forth)
	// For float we need 4 iterations (53 bit mantissa)
	for (int i = 0; i < 4; i++) {
		r = (2. / 3.) * r + (1. / 3.) * x / (r * r);
	}

	return r;
}

long double cbrtl(long double x) {
	// Argument reduction
	int exp;
	long double fr = frexpl(x, &exp);
	int shx = exp % 3;
	if (shx > 0) {
		// Compute shx such that (exp - shx) is divisible by 3
		shx -= 3;
	}
	exp = (exp - shx) / 3; // exponent of cube root
	fr = ldexpl(fr, shx);
	// 0.125 <= fr < 1.0

	// Compute seed with a quadratic approximation
	fr = (-0.46946116L * fr + 1.072302L) * fr + 0.3812513L;	// 0.5 <= fr < 1
	long double r = ldexpl(fr, exp);	// 6 bits of precision

	// Use Newton-Raphson iterations to improve precision
	// Each iterations doubles the precision (e.g. after 1. iterations we have 12 bits, after 2. we have 24 and so forth)
	// For float we need 4 iterations (63 bit mantissa)
	for (int i = 0; i < 4; i++) {
		r = (2.L / 3.L) * r + (1.L / 3.L) * x / (r * r);
	}

	return r;
}

float hypotf(float x, float y) {
	if (isinf(x) || isinf(y)) return INFINITY;
	return sqrtf(x * x + y * y);
}

double hypot(double x, double y) {
	if (isinf(x) || isinf(y)) return INFINITY;
	return sqrt(x * x + y * y);
}

long double hypotl(long double x, long double y) {
	if (isinf(x) || isinf(y)) return INFINITY;
	return sqrtl(x * x + y * y);
}

// ----- Trigonometric functions -----

float sinf(float x) {
	return sinl(x);
}

double sin(double x) {
	return sinl(x);
}

long double sinl(long double x) {
	if (isinf(x)) return NAN;
	// FIXME: x needs to be in the range of -2^63 to 2^63 for fsin to work
	long double res;
	asm("fsin;" :"=t"(res) :"0"(x));
	return res;
}

float cosf(float x) {
	return cosl(x);
}

double cos(double x) {
	return cosl(x);
}

long double cosl(long double x) {
	if (isinf(x)) return NAN;
	// FIXME: x needs to be in the range of -2^63 to 2^63 for fcos to work
	long double res;
	asm("fcos;" :"=t"(res) :"0"(x));
	return res;
}

float tanf(float x) {
	return tanl(x);
}

double tan(double x) {
	return tanl(x);
}

long double tanl(long double x) {
	if (isinf(x)) return NAN;
	// FIXME: x needs to be in the range of -2^63 to 2^63 for fptan to work
	long double res;
	asm("fptan;fstp %%st;" :"=t"(res) :"0"(x));
	return res;
}

float asinf(float x) {
	return atanf(x / sqrtf(1.f - x * x));
}

double asin(double x) {
	return atan(x / sqrt(1 - x * x));
}

long double asinl(long double x) {
	return atanl(x / sqrtl(1 - x * x));
}

float acosf(float x) {
	return atanf(sqrtf(1.f - x * x) / x);
}

double acos(double x) {
	return atan(sqrt(1 - x * x) / x);
}

long double acosl(long double x) {
	return atanl(sqrtl(1 - x * x) / x);
}

float atanf(float x) {
	return atanl(x);
}

double atan(double x) {
	return atanl(x);
}

long double atanl(long double x) {
	return atan2l(x, 1);
}

float atan2f(float x, float y) {
	return atan2l(x, y);
}

double atan2(double x, double y) {
	return atan2l(x, y);
}

long double atan2l(long double x, long double y) {
	long double res;
	asm("fpatan" :"=t"(res) :"0"(x), "u"(y): "st(1)");
	return res;
}

// ----- Hyperbolic functions -----

float sinhf(float x) {
	return (expf(x) - expf(-x)) / 2.f;
}

double sinh(double x) {
	return (exp(x) - exp(-x)) / 2;
}

long double sinhl(long double x) {
	return (expl(x) - expl(-x)) / 2;
}

float coshf(float x) {
	return (expf(x) + expf(-x)) / 2.f;
}

double cosh(double x) {
	return (exp(x) + exp(-x)) / 2;
}

long double coshl(long double x) {
	return (expl(x) + expl(-x)) / 2;
}

float tanhf(float x) {
	if (isinf(x)) return copysignf(1, x);
	float e = expf(2.f * x);
	return (e - 1.f) / (e + 1.f);
}

double tanh(double x) {
	if (isinf(x)) return copysign(1, x);
	double e = exp(2 * x);
	return (e - 1) / (e + 1);
}

long double tanhl(long double x) {
	if (isinf(x)) return copysignl(1, x);
	long double e = expl(2 * x);
	return (e - 1) / (e + 1);
}

float asinhf(float x) {
	return logf(x + sqrtf(x * x + 1.f));
}

double asinh(double x) {
	return log(x + sqrt(x * x + 1));
}

long double asinhl(long double x) {
	return logl(x + sqrtl(x * x + 1));
}

float acoshf(float x) {
	return logf(x + sqrtf(x * x - 1.f));
}

double acosh(double x) {
	return log(x + sqrt(x * x - 1));
}

long double acoshl(long double x) {
	return logl(x + sqrtl(x * x - 1));
}

float atanhf(float x) {
	return 0.5f * logf((1.f + x) / (1.f - x));
}

double atanh(double x) {
	return 0.5 * log((1 + x) / (1 - x));
}

long double atanhl(long double x) {
	return 0.5 * logl((1 + x) / (1 - x));
}

// Nearest integer floating-point operations

enum fpu_rounding_mode {
	FPU_ROUND_NEAREST	= 0b00,
	FPU_ROUND_DOWN		= 0b01,
	FPU_ROUND_UP		= 0b10,
	FPU_ROUND_TRUNC		= 0b11
};

static long double fpu_round(long double x, enum fpu_rounding_mode mode) {
	uint16_t control_word;
	// Save current control word
	asm("fnstcw %0": "=m"(control_word));

	// Do the rounding in the specified mode
	uint16_t tmp_control_word = (control_word & ~(0b11 << 10)) | (mode << 10);
	asm("fldcw %[control_word];"
		"frndint;"
		: "=t"(x): "0"(x), [control_word]"m"(tmp_control_word));

	// Restore control word
	asm("fldcw %0":: [control_word]"m"(control_word));
	return x;
}

float ceilf(float x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_ceil_ss(TO_SSE(x), TO_SSE(x)));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	float_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 127;
	if (exp < 0) {
		// < 2^0
		return tmp.sign ? -0. : 1;
	}
	if (exp < FLT_MANT_DIG - 1) {
		// zero out mantissa after . and add 1
		uint32_t mask = UINT32_MAX << (FLT_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
		if (!tmp.sign) tmp.f++;
	}
	return tmp.f;
#endif
}

double ceil(double x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_ceil_sd(TO_SSE(x), TO_SSE(x)));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	double_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 1023;
	if (exp < 0) {
		// < 2^0
		return tmp.sign ? -0. : 1;
	}
	if (exp < DBL_MANT_DIG - 1) {
		// zero out mantissa after . and add 1
		uint64_t mask = UINT64_MAX << (DBL_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
		if (!tmp.sign) tmp.f++;
	}
	return tmp.f;
#endif
}

long double ceill(long double x) {
	return fpu_round(x, FPU_ROUND_UP);
}

float floorf(float x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_floor_ss(TO_SSE(x), TO_SSE(x)));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	float_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 127;
	if (exp < 0) {
		// < 2^0
		return tmp.sign ? -1 : 0;
	}
	if (exp < FLT_MANT_DIG - 1) {
		// zero out mantissa after . and subtract 1
		uint32_t mask = UINT32_MAX << (FLT_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
		if (tmp.sign) tmp.f--;
	}
	return tmp.f;
#endif
}

double floor(double x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_floor_sd(TO_SSE(x), TO_SSE(x)));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	double_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 1023;
	if (exp < 0) {
		// < 2^0
		return tmp.sign ? -1 : 0;
	}
	if (exp < DBL_MANT_DIG - 1) {
		// zero out mantissa after . and subtract 1
		uint64_t mask = UINT64_MAX << (DBL_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
		if (tmp.sign) tmp.f--;
	}
	return tmp.f;
#endif
}

long double floorl(long double x) {
	return fpu_round(x, FPU_ROUND_DOWN);
}

float truncf(float x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_round_ss(TO_SSE(x), TO_SSE(x), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	float_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 127;
	if (exp < 0) {
		// < 2^0
		return copysignf(0, x);
	}
	if (exp < FLT_MANT_DIG - 1) {
		// zero out mantissa after .
		uint32_t mask = UINT32_MAX << (FLT_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
	}
	return tmp.f;
#endif
}

double trunc(double x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_round_sd(TO_SSE(x), TO_SSE(x), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	double_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 1023;
	if (exp < 0) {
		// < 2^0
		return copysign(0, x);
	}
	if (exp < DBL_MANT_DIG - 1) {
		// zero out mantissa after .
		uint64_t mask = UINT64_MAX << (DBL_MANT_DIG - 1 - exp);
		tmp.mantissa &= mask;
	}
	return tmp.f;
#endif
}

long double truncl(long double x) {
	return fpu_round(x, FPU_ROUND_TRUNC);
}

float roundf(float x) {
	return signbit(x) ? ceilf(x - 0.5f) : floorf(x + 0.5f);
}

double round(double x) {
	return signbit(x) ? ceil(x - 0.5) : floor(x + 0.5);
}

long double roundl(long double x) {
	return signbit(x) ? ceill(x - 0.5L) : floorl(x + 0.5L);
}

long lroundf(float x) {
	return roundf(x);
}

long lround(double x) {
	return round(x);
}

long lroundl(long double x) {
	return roundl(x);
}

long long llroundf(float x) {
	return roundf(x);
}

long long llround(double x) {
	return round(x);
}

long long llroundl(long double x) {
	return roundl(x);
}

float roundevenf(float x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_round_ss(TO_SSE(x), TO_SSE(x), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	float_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 127;
	if (exp < -1) {
		// < 2^-1
		return copysignf(0, x);
	}
	if (exp < FLT_MANT_DIG - 1) {
		// zero out mantissa after . and check last bit
		uint32_t mask = UINT32_MAX << (FLT_MANT_DIG - 1 - exp);
		bool round_bit = (tmp.mantissa >> (FLT_MANT_DIG - 1 - (exp + 1))) & 1;
		bool last_bit = exp > 0 ? ((tmp.mantissa >> (FLT_MANT_DIG - 1 - exp)) & 1) : 1;
		tmp.mantissa &= mask;
		// Round half to even
		if (round_bit && last_bit) {
			tmp.f += tmp.sign ? -1.f : 1.f;
		}
	}
	return tmp.f;
#endif
}

double roundeven(double x) {
#ifdef __SSE4_1__
	return FROM_SSE(_mm_round_sd(TO_SSE(x), TO_SSE(x), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
#else
	if (isnan(x) || isinf(x) || x == 0) return x;
	double_struct_t tmp = {
		.f = x
	};
	int exp = tmp.exponent - 1023;
	if (exp < -1) {
		// < 2^-1
		return copysign(0, x);
	}
	if (exp < DBL_MANT_DIG - 1) {
		// zero out mantissa after . and check last bit
		uint64_t mask = UINT64_MAX << (DBL_MANT_DIG - 1 - exp);
		bool round_bit = (tmp.mantissa >> (DBL_MANT_DIG - 1 - (exp + 1))) & 1;
		bool last_bit = exp > 0 ? ((tmp.mantissa >> (DBL_MANT_DIG - 1 - exp)) & 1) : 1;
		tmp.mantissa &= mask;
		// Round half to even
		if (round_bit && last_bit) {
			tmp.f += tmp.sign ? -1 : 1;
		}
	}
	return tmp.f;
#endif
}

long double roundevenl(long double x) {
	return fpu_round(x, FPU_ROUND_NEAREST);
}


// Error and gamma functions

// TODO: improve accuracy of erf and erfc
// Error function: erf(x) = 2/sqrt(pi) * sum_{n=0}^{inf}((-1)^n * z^(2n + 1) / (n! * (2n + 1)))
// We are using the Bürmann series: https://en.wikipedia.org/wiki/Error_function#Bürmann_series
float erff(float x) {
	if (isnan(x) || x == 0) {
		return x;
	}
	if (isinf(x)) {
		return copysignf(1, x);
	}

	const float sqrt_pi = 1.7724538509055160272981674833411f;
	float ex2 = expf(-(x * x));
	float sum = sqrt_pi / 2.f + (31.f / 200.f - 341.f / 8000.f * ex2) * ex2;

	return copysignf((2.f / sqrt_pi) * sqrtf(1.f - ex2) * sum, x);
}

double erf(double x) {
	if (isnan(x) || x == 0) {
		return x;
	}
	if (isinf(x)) {
		return copysign(1, x);
	}

	const double sqrt_pi = 1.7724538509055160272981674833411;
	double ex2 = exp(-(x * x));
	double sum = sqrt_pi / 2 + (31. / 200. - 341. / 8000. * ex2) * ex2;

	return copysign((2 / sqrt_pi) * sqrt(1 - ex2) * sum, x);
}

long double erfl(long double x) {
	if (isnan(x) || x == 0) {
		return x;
	}
	if (isinf(x)) {
		return copysignl(1, x);
	}

	const long double sqrt_pi = 1.7724538509055160272981674833411L;
	long double ex2 = expl(-(x * x));
	long double sum = sqrt_pi / 2 + (31. / 200. - 341. / 8000. * ex2) * ex2;

	return copysignl((2 / sqrt_pi) * sqrtl(1 - ex2) * sum, x);
}

// Complementary error function: erfc(x) = 1 - erf(x)
float erfcf(float x) {
	return 1.f - erff(x);
}

double erfc(double x) {
	return 1 - erf(x);
}

long double erfcl(long double x) {
	return 1.L - erfl(x);
}

// Floating-point manipulation functions

float frexpf(float x, int *exp) {
	*exp = (x == 0) ? 0 : (1 + ilogbf(x));
	return scalbnf(x, -(*exp));
}

double frexp(double x, int *exp) {
	*exp = (x == 0) ? 0 : (1 + ilogb(x));
	return scalbn(x, -(*exp));
}

long double frexpl(long double x, int *exp) {
	*exp = (x == 0) ? 0 : (1 + ilogbl(x));
	return scalbnl(x, -(*exp));
}

float ldexpf(float x, int exp) {
	return scalblnf(x, exp);
}

double ldexp(double x, int exp) {
	return scalbln(x, exp);
}

long double ldexpl(long double x, int exp) {
	return scalblnl(x, exp);
}

float modff(float x, float *iptr) {
	*iptr = roundevenf(x);
	return copysignf(isinf(x) ? 0.0 : x - (*iptr), x);
}

double modf(double x, double *iptr) {
	*iptr = roundeven(x);
	return copysign(isinf(x) ? 0.0 : x - (*iptr), x);
}

long double modfl(long double x, long double *iptr) {
	*iptr = roundevenl(x);
	return copysignl(isinf(x) ? 0.0 : x - (*iptr), x);
}

// These helper functions return the (negated) exponent of a denormal floating-point number (which needs to be added to the regular exponent)
// This means the smaller the number the bigger is the returned exponent
// The functions assume that the passed number is a denormalized number
static unsigned float_subnormal_exp(float_struct_t f) {
	return __builtin_clz(f.mantissa) - (sizeof(uint32_t) * CHAR_BIT - (FLT_MANT_DIG - 1));
}

static unsigned double_subnormal_exp(double_struct_t f) {
	return __builtin_clzl(f.mantissa) - (sizeof(uint64_t) * CHAR_BIT - (DBL_MANT_DIG - 1));
}

static unsigned long_double_subnormal_exp(long_double_struct_t f) {
	return __builtin_clzl(f.mantissa) - (sizeof(uint64_t) * CHAR_BIT - LDBL_MANT_DIG - 1 + 1) - 1;
}

float scalbnf(float x, int exp) {
	return scalblnf(x, exp);
}

double scalbn(double x, int exp) {
	return scalbln(x, exp);
}

long double scalbnl(long double x, int exp) {
	return scalblnl(x, exp);
}

float scalblnf(float x, long exp) {
	const unsigned mantissa_bits = FLT_MANT_DIG - 1;
	if (isnan(x) || isinf(x) || x == 0 || exp == 0) return x;
	float_struct_t cvt;
	cvt.f = x;
	long new_exp = cvt.exponent + exp;
	unsigned current_mantissa_shift = 0;
	if (cvt.exponent == 0) {
		current_mantissa_shift = float_subnormal_exp(cvt) + 1;
		new_exp -= current_mantissa_shift - 1;
	}
	if (new_exp < 256 && new_exp >= -(long)mantissa_bits) {
		unsigned new_mantissa_shift = new_exp <= 0 ? -new_exp + 1 : 0;
		cvt.exponent = new_exp > 0 ? new_exp : 0;
		if (new_mantissa_shift < current_mantissa_shift) {
			cvt.mantissa <<= current_mantissa_shift - new_mantissa_shift;
		} else {
			cvt.mantissa >>= new_mantissa_shift - current_mantissa_shift;
			if (current_mantissa_shift == 0) {
				// "Shift" in the implicit 24th bit
				cvt.mantissa |= 1 << (mantissa_bits - new_mantissa_shift);
			}
		}
	}
	return cvt.f;
}

double scalbln(double x, long exp) {
	const unsigned mantissa_bits = DBL_MANT_DIG - 1;
	if (isnan(x) || isinf(x) || x == 0 || exp == 0) return x;
	double_struct_t cvt;
	cvt.f = x;
	long new_exp = cvt.exponent + exp;
	unsigned current_mantissa_shift = 0;
	if (cvt.exponent == 0) {
		current_mantissa_shift = double_subnormal_exp(cvt) + 1;
		new_exp -= current_mantissa_shift - 1;
	}
	if (new_exp < 2048 && new_exp >= -(long)mantissa_bits) {
		unsigned new_mantissa_shift = new_exp <= 0 ? -new_exp + 1 : 0;
		cvt.exponent = new_exp > 0 ? new_exp : 0;
		if (new_mantissa_shift < current_mantissa_shift) {
			cvt.mantissa <<= current_mantissa_shift - new_mantissa_shift;
		} else {
			cvt.mantissa >>= new_mantissa_shift - current_mantissa_shift;
			if (current_mantissa_shift == 0) {
				// "Shift" in the implicit 54th bit
				cvt.mantissa |= 1ul << (mantissa_bits - new_mantissa_shift);
			}
		}
	}
	return cvt.f;
}

long double scalblnl(long double x, long exp) {
	long double res;
	asm("fildq %[exp];"
		"fldt %[x];"
		"fscale": "=t"(res): [x]"m"(x), [exp]"m"(exp));
	return res;
}

static int ilogbf_nocheck(float x) {
	float_struct_t cvt;
	cvt.f = x;
	int exp = cvt.exponent;
	if (exp == 0) {
		// Denomal value
		exp -= float_subnormal_exp(cvt);
	}
	return exp - 127;
}

static int ilogb_nocheck(double x) {
	double_struct_t cvt;
	cvt.f = x;
	int exp = cvt.exponent;
	if (exp == 0) {
		// Denomal value
		exp -= double_subnormal_exp(cvt);
	}
	return exp - 1023;
}

static int ilogbl_nocheck(long double x) {
	long_double_struct_t cvt;
	cvt.f = x;
	int exp = cvt.exponent;
	if (exp == 0) {
		// Denomal value
		exp -= long_double_subnormal_exp(cvt);
	}
	return exp - 16383;
}

int ilogbf(float x) {
	if (x == 0) return FP_ILOGB0;
	if (isnan(x)) return FP_ILOGBNAN;
	if (isinf(x)) return INT_MAX;
	return ilogbf_nocheck(x);
}

int ilogb(double x) {
	if (x == 0) return FP_ILOGB0;
	if (isnan(x)) return FP_ILOGBNAN;
	if (isinf(x)) return INT_MAX;
	return ilogb_nocheck(x);
}

int ilogbl(long double x) {
	if (x == 0) return FP_ILOGB0;
	if (isnan(x)) return FP_ILOGBNAN;
	if (isinf(x)) return INT_MAX;
	return ilogbl_nocheck(x);
}

float logbf(float x) {
	if (x == 0) return -INFINITY;
	if (isnan(x)) return NAN;
	if (isinf(x)) return INFINITY;
	return ilogbf_nocheck(x);
}

double logb(double x) {
	if (x == 0) return -INFINITY;
	if (isnan(x)) return NAN;
	if (isinf(x)) return INFINITY;
	return ilogb_nocheck(x);
}

long double logbl(long double x) {
	if (x == 0) return -INFINITY;
	if (isnan(x)) return NAN;
	if (isinf(x)) return INFINITY;
	return ilogbl_nocheck(x);
}

float nextafterf(float from, float to) {
	if (from == to) return to;
	if (isnan(from) || isnan(to)) return NAN;
	float_struct_t cvt;
	cvt.f = from;
    if (signbit(from)) {
        cvt.i += to > from ? -1 : 1;
    } else {
        cvt.i += to > from ? 1 : -1;
    }
	return cvt.f;
}

double nextafter(double from, double to) {
	if (from == to) return to;
	if (isnan(from) || isnan(to)) return NAN;
	double_struct_t cvt;
	cvt.f = from;
    if (signbit(from)) {
        cvt.i += to > from ? -1 : 1;
    } else {
        cvt.i += to > from ? 1 : -1;
    }
	return cvt.f;
}

long double nextafterl(long double from, long double to) {
	if (from == to) return to;
	if (isnan(from) || isnan(to)) return NAN;
	long_double_struct_t cvt;
	cvt.f = from;
    if (signbit(from)) {
        cvt.i += to > from ? -1 : 1;
    } else {
        cvt.i += to > from ? 1 : -1;
    }
	return cvt.f;
}

float copysignf(float x, float y) {
	const uint32_t signmask = 1 << 31;
	__m128 magnitude = _mm_and_ps(TO_SSE(x), _mm_castsi128_ps(_mm_cvtsi32_si128(~signmask)));
	__m128 sign = _mm_and_ps(TO_SSE(y), _mm_castsi128_ps(_mm_cvtsi32_si128(signmask)));
	return FROM_SSE(_mm_or_ps(magnitude, sign));
}

double copysign(double x, double y) {
	const uint64_t signmask = 1ul << 63;
	__m128d magnitude = _mm_and_pd(TO_SSE(x), _mm_castsi128_pd(_mm_cvtsi64_si128(~signmask)));
	__m128d sign = _mm_and_pd(TO_SSE(y), _mm_castsi128_pd(_mm_cvtsi64_si128(signmask)));
	return FROM_SSE(_mm_or_pd(magnitude, sign));
}

long double copysignl(long double x, long double y) {
	const uint128_t signmask = (uint128_t)1 << 79;
	union {
		long double f;
		uint128_t i;
	} tmp_x, tmp_y;
	tmp_x.f = x;
	tmp_y.f = y;

	uint128_t magnitude = tmp_x.i & ~signmask;
	uint128_t sign = tmp_y.i & signmask;
	tmp_x.i = magnitude | sign;
	return tmp_x.f;
}
