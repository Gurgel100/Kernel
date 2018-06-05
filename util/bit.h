/*
 * bit.h
 *
 *  Created on: 05.06.2018
 *      Author: pascal
 */

#ifndef BIT_H_
#define BIT_H_

#define BIT_MASK(bit)		(1ul << bit)
#define BIT_EXTRACT(x, bit) (!!(x & BIT_MASK(bit)))
#define BIT_SET(x, bit)		(x | BIT_MASK(bit))
#define BIT_CLEAR(x, bit)	(x & ~BIT_MASK(bit))

#endif /* BIT_H_ */
