/*
 * math.h
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#ifndef MATH_H_
#define MATH_H_

#include <limits.h>
#include <float.h>

#define _DECL_UNARYFUNC(name)									\
	extern float name##f(float x);								\
	extern double name(double x);								\
	extern long double name##l(long double x);
#define _DECL_UNARYFUNC_EXP(name, type)							\
	extern float name##f(float x, type exp);					\
	extern double name(double x, type exp);						\
	extern long double name##l(long double x, type exp);
#define _DECL_UNARYFUNC_RET(name, type)							\
	extern type name##f(float x);								\
	extern type name(double x);									\
	extern type name##l(long double x);

#define _DECL_BINFUNC(name)										\
	extern float name##f(float x, float y);						\
	extern double name(double x, double y);						\
	extern long double name##l(long double x, long double y);

#ifdef __cplusplus
extern "C" {
#endif

#define M_E         			2.71828182845904523536
#define M_PI					3.14159265358979323846
#define NAN     	    		__builtin_nanf("")
#define INFINITY				__builtin_inff()
#define HUGE_VALF				__builtin_huge_valf()
#define HUGE_VAL				__builtin_huge_val()
#define HUGE_VALL				__builtin_huge_vall()

#define FP_NORMAL				0
#define FP_SUBNORMAL			1
#define FP_ZERO					2
#define FP_INFINITE				3
#define FP_NAN					4
#define fpclassify(x)			__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isfinite(x)				__builtin_isfinite(x)
#define isinf(x)				__builtin_isinf(x)
#define isnan(x)				__builtin_isnan(x)
#define isnormal(x)				__builtin_isnormal(x)
#define signbit(x)				__builtin_signbit(x)
#define isgreater(x, y)			__builtin_isgreater(x, y)
#define isgreaterequal(x, y)	__builtin_isgreaterequal(x, y)
#define isless(x, y)			__builtin_isless(x, y)
#define islessequal(x, y)		__builtin_islessequal(x, y)
#define islessgreater(x, y)		__builtin_islessgreater(x, y)
#define isunordered(x, y)		__builtin_isunordered(x, y)

#define FP_ILOGB0				INT_MIN
#define FP_ILOGBNAN				INT_MIN

#if FLT_EVAL_METHOD == 0
typedef float float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 1
typedef double float_t;
typedef double double_t;
#elif FLT_EVAL_METHOD == 2
typedef long double float_t;
typedef long double double_t;
#else
#error "Unknown FLT_EVAL_METHOD"
#endif

_DECL_UNARYFUNC(fabs)
_DECL_BINFUNC(fmod)
_DECL_BINFUNC(remainder)
_DECL_BINFUNC(fmax)
_DECL_BINFUNC(fmin)
_DECL_BINFUNC(fdim)
extern float nanf(const char *arg);
extern double nan(const char *arg);
extern long double nanl(const char *arg);

_DECL_UNARYFUNC(exp)
_DECL_UNARYFUNC(log)
_DECL_UNARYFUNC(log10)
_DECL_UNARYFUNC(log2)
_DECL_BINFUNC(pow)
_DECL_UNARYFUNC(sqrt)
_DECL_UNARYFUNC(cbrt)
_DECL_BINFUNC(hypot)

_DECL_UNARYFUNC(sin)
_DECL_UNARYFUNC(cos)
_DECL_UNARYFUNC(tan)
_DECL_UNARYFUNC(asin)
_DECL_UNARYFUNC(acos)
_DECL_UNARYFUNC(atan)
_DECL_BINFUNC(atan2)
_DECL_UNARYFUNC(sinh)
_DECL_UNARYFUNC(cosh)
_DECL_UNARYFUNC(tanh)

// Error and gamma functions
_DECL_UNARYFUNC(erf)
_DECL_UNARYFUNC(erfc)

_DECL_UNARYFUNC(ceil)
_DECL_UNARYFUNC(floor)
_DECL_UNARYFUNC(trunc)
_DECL_UNARYFUNC(round)
_DECL_UNARYFUNC_RET(lround, long)
_DECL_UNARYFUNC_RET(llround, long long)
_DECL_UNARYFUNC(roundeven)

// Floating-point manipulation functions
_DECL_UNARYFUNC_EXP(frexp, int*)
_DECL_UNARYFUNC_EXP(ldexp, int)
float modff(float x, float *iptr);
double modf(double x, double *iptr);
long double modfl(long double x, long double *iptr);
_DECL_UNARYFUNC_EXP(scalbn, int)
_DECL_UNARYFUNC_EXP(scalbln, long)
extern int ilogbf(float x);
extern int ilogb(double x);
extern int ilogbl(long double x);
_DECL_UNARYFUNC(logb)
float nextafterf(float from, float to);
double nextafter(double from, double to);
long double nextafterl(long double from, long double to);
_DECL_BINFUNC(copysign)

#ifdef __cplusplus
}
#endif

#undef _DECL_UNARYFUNC
#undef _DECL_BINFUNC

#endif /* MATH_H_ */
