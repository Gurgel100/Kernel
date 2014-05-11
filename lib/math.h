/*
 * math.h
 *
 *  Created on: 19.08.2012
 *      Author: pascal
 */

#ifndef MATH_H_
#define MATH_H_

#ifdef __cplusplus
extern "C" {
#endif

#define M_E         2.71828182845904523536
#define NAN         (__builtin_nanf (""))

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

#ifdef __cplusplus
}
#endif

#endif /* MATH_H_ */
