/*
 * math.c
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#include "math.h"

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

double fabs(double x)
{
	return (x >= 0) ? x : -x;
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
