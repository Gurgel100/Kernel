/*
 * hash_helpers.h
 *
 *  Created on: 07.01.2018
 *      Author: pascal
 */

#ifndef HASH_HELPERS_H_
#define HASH_HELPERS_H_

#include <stdint.h>
#include <stdbool.h>

uint64_t string_hash(const void *key, void *context);
bool string_equal(const void *a, const void *b, void *context);

#endif /* HASH_HELPERS_H_ */
