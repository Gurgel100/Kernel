/*
 * hash_helpers.c
 *
 *  Created on: 07.01.2018
 *      Author: pascal
 */

#include "hash_helpers.h"
#include <stddef.h>
#include <string.h>

//Murmur2 hash taken from https://github.com/aappleby/smhasher
uint64_t string_hash(const void *key, __attribute__((unused)) void *context)
{
	size_t len = strlen((const char*)key);
	const uint64_t m = 0xc6a4a7935bd1e995lu;
	const int r = 47;

	uint64_t h = len * m;

	const uint64_t *data = (const uint64_t*)key;
	const uint64_t *end = data + (len / 8);

	while(data != end)
	{
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char *data2 = (const unsigned char*)data;

	switch(len & 7)
	{
		case 7: h ^= (uint64_t)data2[6] << 48;
		/* no break */
		case 6: h ^= (uint64_t)data2[5] << 40;
		/* no break */
		case 5: h ^= (uint64_t)data2[4] << 32;
		/* no break */
		case 4: h ^= (uint64_t)data2[3] << 24;
		/* no break */
		case 3: h ^= (uint64_t)data2[2] << 16;
		/* no break */
		case 2: h ^= (uint64_t)data2[1] << 8;
		/* no break */
		case 1: h ^= (uint64_t)data2[0];
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}

bool string_equal(const void *a, const void *b, __attribute__((unused)) void *context)
{
	const char *stra = a;
	const char *strb = b;
	return strcmp(stra, strb) == 0;
}
