/*
 * math.c
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#include "math.h"
#include <stdint.h>
#include <immintrin.h>

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

#define BINOP(op, x, y)	FROM_SSE(OP_SUFFIX(CONCAT(_mm_, op), x)(TO_SSE(x), TO_SSE(y)))

#define DEFINE_BINOP(name, type, op)	\
	type name(type x, type y) {			\
		return BINOP(op, x, y);			\
	}

double sin(double x)
{
	double Ergebnis;
	asm("fsin;" :"=t"(Ergebnis) :"0"(x));
	return Ergebnis;
}

double cos(double x)
{
	double Ergebnis;
	asm("fcos;" :"=t"(Ergebnis) :"0"(x));
	return Ergebnis;
}

double tan(double x)
{
	double Ergebnis;
	asm("fptan;fstp %%st;" :"=t"(Ergebnis) :"0"(x));
	return Ergebnis;
}

double asin(double x)
{
	if(x < -1 || x > 1) return NAN;
	return atan(x / sqrt(1 - x * x));
}

double acos(double x)
{
	if(x < -1 || x > 1) return NAN;
	return atan(sqrt(1 - x * x) / x);
}

double atan(double x)
{
	if(x < -1 || x > 1) return NAN;
	double Ergebnis;
	asm("fld1;fpatan;" :"=t"(Ergebnis) :"0"(x));
	return Ergebnis;
}

double atan2(double x, double y)
{
	if(x > 0)
		return atan(y / x);
	else if(x < 0)
	{
		double t = atan(y / x);
		if(y >= 0)
			return t + M_PI;
		else
			return t - M_PI;
	}
	else
	{
		if(y < 0)
			return M_PI / 2;
		else					//x = 0 & y = 0 ergibt ein undefiniertes Resultat
			return -M_PI / 2;
	}
}

double sinh(double x)
{
	return (exp(x) - exp(-x)) / 2;
}
double cosh(double x)
{
	return (exp(x) + exp(-x)) / 2;
}
double tanh(double x)
{
	double e = exp(2 * x);
	return (e - 1) / (e + 1);
}

/*
 * e hoch x
 */
double exp(double x)
{
	double Ergebnis;
	asm volatile(
			"fldl2e;"
			"fmulp;"
			"f2xm1;"
			:"=t"(Ergebnis) :"0"(x)
	);
	return Ergebnis + 1;
}

double log(double x)	//Natürlicher Logarithmus von x ln(x)
{
	if(x <= 0) return NAN;
	double Ergebnis;
	asm(
			"fld1;"
			"fyl2x;"
			"fldl2e;"
			"fdiv %%st(0),%%st(1)"
			:"=t"(Ergebnis) :"0"(x)
	);
	return Ergebnis;
}

double log10(double x)	//Logarithmus zur Basis 10 von x lg(x)
{
	if(x <= 0) return NAN;
	double Ergebnis;
	asm(
			"fld1;"
			"fyl2x;"
			"fldl2t;"
			"fdiv %%st(0),%%st(1)"
			:"=t"(Ergebnis) :"0"(x)
	);
	return Ergebnis;
}

double pow(double x, double y)
{
	return exp(y * log(x));
}

double sqrt(double x)
{
	double Ergebnis;
	asm("sqrtsd %[x],%[res]": [res]"=x"(Ergebnis): [x]"xm"(x));
	return Ergebnis;
}

double ceil(double x)
{
#ifdef __SSE4_1__
	double Ergebnis;
	asm("roundsd %[mode],%[x],%[res]": [res]"=x"(Ergebnis): [x]"x"(x), [mode]"K"(0b10));
	return Ergebnis;
#else
	//TODO: funktioniert nur für "normale" Zahlen
	long int i = (long int)x;
	if(i < x) i++;
	return (double)i;
#endif
}

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

double floor(double x)
{
#ifdef __SSE4_1__
	double Ergebnis;
	asm("roundsd %[mode],%[x],%[res]": [res]"=x"(Ergebnis): [x]"x"(x), [mode]"K"(0b01));
	return Ergebnis;
#else
	//TODO: funktioniert nur für "normale" Zahlen
	return (double)(long int)x;
#endif
}

double fmod(double x, double y)
{
	double Ergebnis;
	asm("fprem" :"=t"(Ergebnis) :"u"(y), "0"(x));
	return Ergebnis;
}
