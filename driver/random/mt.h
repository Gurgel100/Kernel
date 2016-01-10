/*
 * mt.h
 *
 *  Created on: 17.12.2015
 *      Author: pascal
 */

#ifndef MT_H_
#define MT_H_

#include <stdint.h>

void seedMT(unsigned long long seed);
uint64_t randomMT(void);

#endif /* MT_H_ */
