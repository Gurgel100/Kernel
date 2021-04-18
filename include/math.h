/*
 * math.h
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#ifndef MATH_H_
#define MATH_H_

#define _DECL_UNARYFUNC(name)									\
	extern float name##f(float x);								\
	extern double name(double x);								\
	extern long double name##l(long double x);

#define _DECL_BINFUNC(name)										\
	extern float name##f(float x, float y);						\
	extern double name(double x, double y);						\
	extern long double name##l(long double x, long double y);

#ifdef __cplusplus
extern "C" {
#endif

#define M_E         	2.71828182845904523536
#define M_PI			3.14159265358979323846
#define NAN         	(__builtin_nanf (""))
#define INFINITY		(__builtin_inff ())
#define isnan(x)		__builtin_isnan(x)
#define isinf(x)		__builtin_isinf(x)
#define isnormal(x)		__builtin_isnormal(x)
#define isfinite(x)		__builtin_isfinite(x)
#define signbit(x)		__builtin_signbit(x)

#define FP_NORMAL		0
#define FP_SUBNORMAL	1
#define FP_ZERO			2
#define FP_INFINITE		3
#define FP_NAN			4
#define fpclassify(x)	__builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)

extern double sin(double x);
extern double cos(double x);
extern double tan(double x);
extern double asin(double x);
extern double acos(double x);
extern double atan(double x);
extern double atan2(double x, double y);
extern double sinh(double x);
extern double cosh(double x);
extern double tanh(double x);

extern double exp(double x);
extern double log(double x);	//Natürlicher Logarithmus von x ln(x)
extern double log10(double x);	//Logarithmus zur Basis 10 von x lg(x)
extern double pow(double x, double y);
extern double sqrt(double x);

extern double ceil(double x);

_DECL_UNARYFUNC(fabs)

_DECL_BINFUNC(fmax)
_DECL_BINFUNC(fmin)

_DECL_BINFUNC(fdim)

extern float nanf(const char *arg);
extern double nan(const char *arg);
extern long double nanl(const char *arg);

extern double floor(double x);
extern double fmod(double x, double y);

#ifdef __cplusplus
}
#endif

#undef _DECL_UNARYFUNC
#undef _DECL_BINFUNC

#endif /* MATH_H_ */
