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

//TODO
/*double atan2(double x, double y);
double sinh(double x);
double cosh(double x);
double tanh(double x);

double exp(double x)
{}

double log(double x);	//Natürlicher Logarithmus von x ln(x)

double log10(double x);	//Logarithmus zur Basis 10 von x lg(x)

double pow(double x, double y)
{}
*/
double sqrt(double x)
{
	double Ergebnis;
	if(x < 0.0)
		return NAN;
	asm("fsqrt" :"=t"(Ergebnis) :"0"(x));
	return Ergebnis;
}
