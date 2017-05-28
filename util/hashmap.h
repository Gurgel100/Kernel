/*
 * hashmap.h
 *
 *  Created on: 12.11.2015
 *      Author: pascal
 */

#ifndef HASHMAP_H_
#define HASHMAP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HASH_MAP_SUCCESS 0
#define HASH_MAP_SUCCESS_FOUND 1

#define HASH_MAP_DUPLICATE_KEY -1
#define HASH_MAP_NO_MEMORY -10

typedef struct hashmap hashmap_t;

hashmap_t* hashmap_create(
	uint64_t (*hash1)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	uint64_t (*hash2)(const void* key, void* context), /*Hash function used for collision resolution. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context), /*Function used to test for key equality.*/
	void (*free_key)(const void* key),
	void (*free_obj)(const void* obj),
	void* context, /*value passed to hash and equality functions as context parameter*/
	void (*free_cntx)(const void* cntx),
	size_t min_size /*expected amount of entries in hashtable*/
);

hashmap_t* hashmap_create_min(
	uint64_t (*hash)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context) /*Function used to test for key equality.*/
);

void hashmap_destroy(hashmap_t* map);

int hashmap_search(hashmap_t* map, const void* key, void** result);
int hashmap_delete(hashmap_t* map, const void* key);
int hashmap_set(hashmap_t* map, const void* key, const void* obj);

size_t hashmap_size(hashmap_t* map);

void hashmap_visit(hashmap_t* map, void (*visitor)(const void* key, const void* obj, void* context), void* context);

#endif /* HASHMAP_H_ */
