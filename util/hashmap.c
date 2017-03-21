#include "hashmap.h"
#include <stdlib.h>
#include <assert.h>

static const uint64_t primes[] = {101, 211, 431, 863, 1733, 3467, 6947, 13901, 27803, 55609, 111227, 222461, 444929, 889871, 1779761, 3559537, 7119103, 14238221, 28476473, 56952947, 113905901, 227811809, 455623621, 911247257, 1822494581, 3644989199, 7289978407, 14579956817, 29159913637, 58319827297, 116639654657};
static const uint64_t h_primes[] = {97, 199, 421, 859, 1723, 3463, 6917, 13883, 27799, 55603, 111217, 222437, 444901, 889829, 1779709, 3559519, 7119067, 14238199, 28476439, 56952943, 113905877, 227811797, 455623607, 911247199, 1822494511, 3644989099, 7289978357, 14579956813, 29159913623, 58319827273, 116639654581};
static const uint8_t prime_count = sizeof(primes) / sizeof(uint64_t);

typedef enum {
	entry_state_empty,
	entry_state_in_use,
	entry_state_deleted = 0
} _hash_map_entry_state_e;

struct _hash_map_entry {
	const void* key;
	const void* entry;
	_hash_map_entry_state_e state;
};

static _hash_map_entry_t* _cleared_entry_array(size_t size) {
	 _hash_map_entry_t* ret = calloc(size, sizeof(_hash_map_entry_t));
	return ret;
}

static void _insert_no_check_empty(hashmap_t* map, const void* key, const void* obj) {
	uint64_t hash = map->hash1(key, map->context) % primes[map->prime_index];
	uint64_t hash2 = map->hash2(key, map->context) % h_primes[map->prime_index] + 1;
	_hash_map_entry_t* entry = &(map->entries[hash]);
	while (entry->state != entry_state_empty) {
		hash = (hash + hash2) % primes[map->prime_index];
		entry = &(map->entries[hash]);
	}
	entry->state = entry_state_in_use;
	entry->entry = obj;
	entry->key = key;
}

static void _rehash(hashmap_t* map) {
	size_t old_size = primes[map->prime_index];
	size_t new_size = primes[++(map->prime_index)];
	_hash_map_entry_t* old_entries = map->entries;
	map->entries = _cleared_entry_array(new_size);
	size_t i;
	for (i = 0; i < old_size; i++) {
		if (old_entries[i].state == entry_state_in_use) {
			_insert_no_check_empty(map, old_entries[i].key, old_entries[i].entry);
		}
	}
}

hashmap_t* hashmap_create(
	uint64_t (*hash1)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	uint64_t (*hash2)(const void* key, void* context), /*Hash function used for collision resolution. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context), /*Function used to test for key equality.*/
	void (*free_key)(const void* key),
	void (*free_obj)(const void* obj),
	void* context, /*value passed to hash and equality functions as context parameter*/
	size_t min_size /*expected amount of entries in hashtable*/
) {
	assert(min_size < UINT64_MAX / 2 && "Requested size too large");
	min_size *= 2;
	assert(hash1 && "Hash functions need to be defined.");
	assert(hash2 && "Hash functions need to be defined.");
	assert(equal && "Equal function needs to be defined.");
	assert(min_size <= primes[prime_count - 1] && "Requested size exeeds maximum supported size.");
	hashmap_t* ret = malloc(sizeof(hashmap_t));
	if (!ret) goto ret_alloc_failed;
	for (ret->prime_index = 0; ret->prime_index < prime_count && primes[ret->prime_index] < min_size; ret->prime_index++);
	ret->entries = _cleared_entry_array(primes[ret->prime_index]);
	if (!ret->entries) goto entry_alloc_failed;
	ret->context = context;
	ret->equal = equal;
	ret->hash1 = hash1;
	ret->hash2 = hash2;
	ret->count = 0;
	ret->free_key = free_key;
	ret->free_obj = free_obj;
	return ret;
entry_alloc_failed:
	free(ret);
ret_alloc_failed:
	return NULL;
}

hashmap_t* hashmap_create_min(
	uint64_t (*hash)(const void* key, void* context), /*Hash function used for lookup / insertion. Has to satisfy equal(key1, key2) => hash(key1) == hash(key2).*/
	bool (*equal)(const void* key1, const void* key2, void* context) /*Function used to test for key equality.*/
) {
	return hashmap_create(hash, hash, equal, NULL, NULL, NULL, 0);
}

int hashmap_search(hashmap_t* map, const void* key, void** result) {
	uint64_t hash = map->hash1(key, map->context) % primes[map->prime_index];
	uint64_t hash2 = map->hash2(key, map->context) % h_primes[map->prime_index] + 1;
	_hash_map_entry_t* entry = &(map->entries[hash]);
	while (entry->state != entry_state_empty) {
		if (entry->state == entry_state_in_use) {
			if (map->equal(key, entry->key, map->context)) {
				if (result) *result = (void*)entry->entry;
				return 1;
			}
		}
		hash = (hash + hash2) % primes[map->prime_index];
		entry = &(map->entries[hash]);
	}
	return 0;
}

int hashmap_delete(hashmap_t* map, const void* key) {
	uint64_t hash = map->hash1(key, map->context) % primes[map->prime_index];
	uint64_t hash2 = map->hash2(key, map->context) % h_primes[map->prime_index] + 1;
	_hash_map_entry_t* entry = &(map->entries[hash]);
	while (entry->state != entry_state_empty) {
		if (entry->state == entry_state_in_use) {
			if (map->equal(key, entry->key, map->context)) {
				if (map->free_key) map->free_key(entry->key);
				if (map->free_obj) map->free_obj(entry->entry);
				entry->state = entry_state_deleted;
				map->count++;
				return 1;
			}
		}
		hash = (hash + hash2) % primes[map->prime_index];
		entry = &(map->entries[hash]);
	}
	return 0;
}

int hashmap_set(hashmap_t* map, const void* key, const void* obj) {
	uint64_t hash = map->hash1(key, map->context) % primes[map->prime_index];
	uint64_t hash2 = map->hash2(key, map->context) % h_primes[map->prime_index] + 1;
	_hash_map_entry_t* entry = &(map->entries[hash]);
	_hash_map_entry_t* del_entry = NULL;
	while (entry->state != entry_state_empty) {
		if (entry->state == entry_state_in_use) {
			if (map->equal(key, entry->key, map->context)) {
				if (map->free_obj) map->free_obj(entry->entry);
				entry->entry = obj;
				return 1;
			}
		} else if (!del_entry) {
			del_entry = entry;
		}
		hash = (hash + hash2) % primes[map->prime_index];
		entry = &(map->entries[hash]);
	}
	if (del_entry) entry = del_entry;
	entry->state = entry_state_in_use;
	entry->entry = obj;
	entry->key = key;
	map->count++;
	if (map->count > 3 * primes[map->prime_index] / 4) _rehash(map);
	return 0;
}
