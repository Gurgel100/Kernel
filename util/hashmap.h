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

struct _hash_map_entry;

typedef struct _hash_map_entry _hash_map_entry_t;

typedef struct {
	uint64_t (*hash1)(const void* key, void* context);
	uint64_t (*hash2)(const void* key, void* context);
	void (*free_key)(const void* key);
	void (*free_obj)(const void* obj);
	bool (*equal)(const void* key1, const void* key2, void* context);
	void* context;
	_hash_map_entry_t* entries;
	uint64_t count;
	uint8_t prime_index;
} hashmap_t;

hashmap_t* hashmap_create(
	uint64_t (*hash1)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	uint64_t (*hash2)(const void* key, void* context), /*Hash function used for collision resolution. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context), /*Function used to test for key equality.*/
	void (*free_key)(const void* key),
	void (*free_obj)(const void* obj),
	void* context, /*value passed to hash and equality functions as context parameter*/
	size_t min_size /*expected amount of entries in hashtable*/
);

hashmap_t* hashmap_create_min(
	uint64_t (*hash)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context) /*Function used to test for key equality.*/
);

int hashmap_search(hashmap_t* map, const void* key, void** result);

int hashmap_delete(hashmap_t* map, const void* key);

int hashmap_set(hashmap_t* map, const void* key, const void* obj);

#endif /* HASHMAP_H_ */
