/*
 * pmm.c
 *
 *  Created on: 09.07.2012
 *      Author: pascal
 */

#include "pmm.h"
#include "config.h"
#include "vmm.h"
#include "memory.h"
#include "multiboot.h"
#include "display.h"
#include "mm.h"
#include "string.h"
#include "stdlib.h"
#include "lock.h"
#include "assert.h"
#include "lock.h"
#ifdef DEBUGMODE
#include "stdio.h"
#endif

#define MAX(a, b)				(((a) > (b)) ? (a) : (b))
#define MIN(a, b)				(((a) < (b)) ? (a) : (b))

#define TREE_INNER_NODE_ENTRIES	((MM_BLOCK_SIZE - sizeof(uint64_t) - sizeof(void*)) / (sizeof(page_range_t) + sizeof(void*)))
#define TREE_LEAF_NODE_ENTRIES	((MM_BLOCK_SIZE - sizeof(uint64_t)) / sizeof(page_range_t))

typedef union {
	struct {
		uint64_t page_nr: 52;	// The number of the first memory page. If this is 0 the range is invalid
		uint64_t size_exp : 6;	// The number of pages in the range. The actual size is calculated with size = 2^size_exp
	};
	uint64_t raw;
} page_range_t;

// Entries (keys) are sorted ascending (from small to big)
typedef struct tree_node {
	struct {
		uint16_t used_keys;
		bool is_leaf;
	} header;
	union {
		struct {
			page_range_t keys[TREE_INNER_NODE_ENTRIES];
			struct tree_node *childs[TREE_INNER_NODE_ENTRIES + 1];
		} inner;
		// We can also use the leaf to access the keys of inner because they are a superset
		struct {
			page_range_t keys[TREE_LEAF_NODE_ENTRIES];
		} leaf;
	};
} tree_node_t;

static_assert(sizeof(tree_node_t) == MM_BLOCK_SIZE, "B-Tree node needs to fit into a page");
static_assert(offsetof(tree_node_t, leaf.keys) == offsetof(tree_node_t, inner.keys), "leaf.keys should have the same offset as inner.keys");
static_assert(TREE_INNER_NODE_ENTRIES / 2 + TREE_INNER_NODE_ENTRIES / 2 + 1 == TREE_INNER_NODE_ENTRIES, "TREE_INNER_NODE_ENTRIES needs to be odd");
static_assert(TREE_LEAF_NODE_ENTRIES / 2 + TREE_LEAF_NODE_ENTRIES / 2 + 1 == TREE_LEAF_NODE_ENTRIES, "TREE_LEAF_NODE_ENTRIES needs to be odd");

//Anfang und Ende des Kernels
extern uint8_t kernel_start;
extern uint8_t kernel_end;

static uint64_t pmm_totalMemory;		//Maximal verfügbarer RAM (physisch)
static uint64_t pmm_totalPages;			//Gesamtanzahl an phys. Pages
static uint64_t pmm_freePages;			//Verfügbarer (freier) physischer Speicher (4kb)
static uint64_t pmm_Kernelsize;			//Grösse des Kernels in Bytes

static lock_t pmm_lock = LOCK_INIT;

static tree_node_t tree_root_node_base __attribute__((aligned(MM_BLOCK_SIZE))) = {
	.header = {
		.is_leaf = true
	}
};
static tree_node_t *tree_root_node = &tree_root_node_base;

static paddr_t getStartPageFromRange(page_range_t a) {
	return a.page_nr * MM_BLOCK_SIZE;
}

static uint32_t log2(size_t x) {
	assert(x != 0);
	return 64 - __builtin_clzl(x) - 1;
}

static page_range_t getPageRange(paddr_t *page, size_t *page_count) {
	assert(*page_count != 0);
	uint32_t power = log2(*page_count);

	page_range_t range = {
		.page_nr = *page / MM_BLOCK_SIZE,
		.size_exp = power
	};

	*page_count -= 1ul << power;
	*page += (1ul << power) * MM_BLOCK_SIZE;
	return range;
}

typedef struct{
	page_range_t left, right;
} page_range_split_result_t;

static page_range_split_result_t splitPageRange(page_range_t range) {
	uint32_t new_exp = 0;
	if (range.size_exp > 0) {
		new_exp = range.size_exp - 1;
	}

	page_range_split_result_t result = {
		.left = {
			.page_nr = range.page_nr,
			.size_exp = new_exp,
		},
		.right = {
			.page_nr = range.size_exp ? (range.page_nr + (1ul << new_exp)) : 0,
			.size_exp = new_exp
		}
	};

	return result;
}

static uint32_t getKeyIndex(const tree_node_t *node, page_range_t range) {
	uint32_t index = 0;
	while (index < node->header.used_keys && range.raw > node->leaf.keys[index].raw) {
		index++;
	}
	return index;
}

static bool isNodeFull(const tree_node_t *node) {
	if (node->header.is_leaf) {
		return node->header.used_keys == TREE_LEAF_NODE_ENTRIES;
	} else {
		return node->header.used_keys == TREE_INNER_NODE_ENTRIES;
	}
}

static void splitNodeImpl(tree_node_t *parent, tree_node_t *node, tree_node_t *new_node, uint32_t parent_index) {
	assert(isNodeFull(node));

	size_t node_entries = node->header.is_leaf ? TREE_LEAF_NODE_ENTRIES : TREE_INNER_NODE_ENTRIES;

	// Copy half of the original node to the new node
	memcpy(new_node->leaf.keys, &node->leaf.keys[node_entries / 2], (node_entries / 2 + 1) * sizeof(page_range_t));
	
	if (!node->header.is_leaf) {
		// Copy right (node_entries + 1) / 2 child pointers to sibling
		memcpy(new_node->inner.childs, &node->inner.childs[node_entries / 2], ((node_entries + 1) / 2 + 1) * sizeof(tree_node_t*));
	}

	node->header.used_keys = node_entries / 2;
	new_node->header.used_keys = node_entries / 2;
	new_node->header.is_leaf = node->header.is_leaf;

	// Insert new node into parent
	memmove(&parent->leaf.keys[parent_index + 1], &parent->leaf.keys[parent_index], (parent->header.used_keys - parent_index) * sizeof(page_range_t));
	parent->inner.keys[parent_index] = node->leaf.keys[node_entries / 2];
	memmove(&parent->inner.childs[parent_index + 2], &parent->inner.childs[parent_index + 1], (parent->header.used_keys - parent_index) * sizeof(tree_node_t*));
	parent->inner.childs[parent_index + 1] = new_node;
	parent->header.used_keys++;
}

/**
 * Splits the root node into two nodes. The necessary pages to this are taken from range. If it is not big enough then a range from the root node is taken.
 * This is done by creating three sets of keys:
 * 1. The first TREE_NODE_ENTRIES / 2 keys remain in node
 * 2. The key with the index TREE_NODE_ENTRIES / 2 is inserted into the new root node as key and is copied to the new node
 * 3. The last TREE_NODE_ENTRIES / 2 is moved to the new node
 */
static page_range_t splitRootNode(page_range_t range) {
	page_range_t ranges[52] = {0};
	paddr_t right_node_page, new_root_node_page;
	// Get a page from the range
	for (uint32_t i = 0; range.size_exp > 1; i++) {
		assert(i + 1 < 52);
		page_range_split_result_t res = splitPageRange(range);
		range = res.left;
		ranges[i] = res.right;
	}

	if (range.size_exp == 0) {
		// Range only holds 1 page so we get another page from a range from the current root node
		// TODO: implement
		assert(false);
	} else {
		assert(range.size_exp == 1);
		right_node_page = getStartPageFromRange(range);
		new_root_node_page = right_node_page + MM_BLOCK_SIZE;
	}

	tree_node_t *right_node = vmm_Map(NULL, right_node_page, 1, VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL | VMM_FLAGS_NX);
	tree_node_t *new_root_node = vmm_Map(NULL, new_root_node_page, 1, VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL | VMM_FLAGS_NX);

	new_root_node->header.used_keys = 0;
	new_root_node->header.is_leaf = false;
	new_root_node->inner.childs[0] = tree_root_node;
	
	splitNodeImpl(new_root_node, tree_root_node, right_node, 0);

	tree_root_node = new_root_node;

	// TODO: insert all newly created ranges
	return ranges[0];
}

/**
 * Splits node into two nodes. The necessary page to this is taken from range.
 * This is done by creating three sets of keys:
 * 1. The first TREE_NODE_ENTRIES / 2 keys remain in node
 * 2. The key with the index TREE_NODE_ENTRIES / 2 is inserted into the parent node as key and is copied to the new node
 * 3. The last TREE_NODE_ENTRIES / 2 is moved to the new node
 */
static page_range_t splitNode(tree_node_t *parent, tree_node_t *node, page_range_t range) {
	assert(!parent->header.is_leaf);
	page_range_t ranges[52] = {0};
	// Get a page from the range
	for (uint32_t i = 0; range.size_exp > 0; i++) {
		assert(i + 1 < 52);
		page_range_split_result_t res = splitPageRange(range);
		range = res.left;
		ranges[i] = res.right;
	}

	paddr_t node_page = getStartPageFromRange(range);
	tree_node_t *new_node = vmm_Map(NULL, node_page, 1, VMM_FLAGS_WRITE | VMM_FLAGS_GLOBAL | VMM_FLAGS_NX);

	page_range_t parent_key;
	if (node->header.is_leaf) {
		parent_key = node->leaf.keys[TREE_LEAF_NODE_ENTRIES / 2];
	} else {
		parent_key = node->inner.keys[TREE_INNER_NODE_ENTRIES / 2];
	}
	uint32_t parent_index = getKeyIndex(parent, parent_key);

	splitNodeImpl(parent, node, new_node, parent_index);

	// TODO: insert all newly created ranges
	return ranges[0];
}

static void insertRange(page_range_t range) {
	// We always have a root node, so we don't have to handle that
	if (isNodeFull(tree_root_node)) {
		// If the root node is full split it
		range = splitRootNode(range);
		// If we used up the range for the creation of the new node we are finished here.
		if (range.raw == 0) return;
	}

	bool inserted = false;
	while (!inserted) {
		tree_node_t *current = tree_root_node;
		while (!current->header.is_leaf) {
			uint32_t index = getKeyIndex(current, range);

			tree_node_t *child = current->inner.childs[index];

			// If the child is full split it
			assert(child != NULL);
			if (isNodeFull(child)) {
				range = splitNode(current, child, range);
				// If we used up the range for the creation of the new node we are finished here.
				if (range.raw == 0) return;

				// Because the range changed we have to begin again
				break;
			}

			// Don't use child as index could have changed in the meantime
			current = current->inner.childs[index];
		}

		// Check if we didn't exited the while loop before reaching a leaf
		if (current->header.is_leaf) {
			uint32_t index = getKeyIndex(current, range);

			// Check if range can be merged with a neighboor
			// TODO: merge across node boundaries
			if (index > 0 && current->leaf.keys[index - 1].raw == range.raw - (1ul << range.size_exp)) {
				// We can merge with the left range. This means we first remove this node without rebalancing the tree.
				range.raw -= 1ul << range.size_exp;
				range.size_exp++;
				// Slide right keys
				memmove(&current->leaf.keys[index - 1], &current->leaf.keys[index], (current->header.used_keys - index) * sizeof(page_range_t));
				current->header.used_keys--;
			} else if (index < current->header.used_keys && current->leaf.keys[index].raw == range.raw + (1ul << range.size_exp)) {
				// We can merge with the left range. This means we first remove this node without rebalancing the tree.
				range.size_exp++;
				// Slide right keys
				memmove(&current->leaf.keys[index], &current->leaf.keys[index + 1], (current->header.used_keys - index - 1) * sizeof(page_range_t));
				current->header.used_keys--;
			} else {
				memmove(&current->leaf.keys[index + 1], &current->leaf.keys[index], (current->header.used_keys - index) * sizeof(page_range_t));
				// Insert range
				current->leaf.keys[index] = range;
				current->header.used_keys++;
				inserted = true;
			}
		}
	}
}

static page_range_t findRange(paddr_t max_address, size_t pages) {
	uint32_t size_exp = log2(pages);
	tree_node_t *current = tree_root_node;
	do {
		uint32_t index;
		for (index = 0; index < current->header.used_keys && current->leaf.keys[index].size_exp < size_exp; index++);
		if (current->header.is_leaf) {
			page_range_t range = current->leaf.keys[index];
			if (index < current->header.used_keys && getStartPageFromRange(range) < max_address) {
				// We have found a range
				// Slide keys to fill now empty spot
				for (uint32_t i = index + 1; i < current->header.used_keys; i++) {
					current->leaf.keys[i - 1] = current->leaf.keys[i];
				}
				current->header.used_keys--;
				return range;
			} else {
				// We didn't find a suitable range
				return (page_range_t){0};
			}
		} else {
			// TODO: rebalance childs if they have not enough entries

			// Keep traversing down the tree
			if (index == current->header.used_keys) {
				current = current->inner.childs[index - 1];
			} else {
				current = current->inner.childs[index];
			}
		}
	} while (current != NULL);
	return (page_range_t){0};
}

static void markPageReserved(paddr_t address) {
	// TODO: implement
	pmm_freePages--;
}

/*
 * Initialisiert die physikalische Speicherverwaltung
 */
bool pmm_Init()
{
	paddr_t phys_kernel_start = vmm_getPhysAddress(&kernel_start);
	paddr_t phys_kernel_end = vmm_getPhysAddress(&kernel_end);
	mmap *map;
	uint32_t mapLength;
	paddr_t maxAddress = 0;

	pmm_Kernelsize = &kernel_end - &kernel_start;
	map = (mmap*)(uintptr_t)MBS->mbs_mmap_addr;
	mapLength = MBS->mbs_mmap_length;

	//"Nachschauen", wieviel Speicher vorhanden ist
	uint32_t maxLength = mapLength / sizeof(mmap);
	pmm_totalMemory = pmm_freePages = 0;
	for(uint32_t i = 0; i < maxLength; i++)
	{
		pmm_totalMemory += map[i].length;
		maxAddress = MAX(map[i].base_addr + map[i].length, maxAddress);
	}

	pmm_totalPages = pmm_totalMemory / MM_BLOCK_SIZE;
	assert(pmm_totalMemory % MM_BLOCK_SIZE == 0);

	//Map analysieren und entsprechende Einträge in die Speicherverwaltung machen
	do
	{
		if(map->type == 1) {
			if (map->base_addr + map->length > phys_kernel_end + MM_BLOCK_SIZE) {
				paddr_t start_page = MAX(map->base_addr, phys_kernel_end + MM_BLOCK_SIZE);
				size_t page_count = (map->base_addr + map->length - start_page) / MM_BLOCK_SIZE;
				while (page_count > 0) {
					page_range_t range = getPageRange(&start_page, &page_count);
					insertRange(range);
				}
			}
		}
		map = (mmap*)((uintptr_t)map + map->size + 4);
	}
	while(map < (mmap*)(uintptr_t)(MBS->mbs_mmap_addr + mapLength));

	#ifdef DEBUGMODE
	printf("    %u MB Speicher gefunden\n    %u GB Speicher gefunden\n", pmm_totalMemory >> 20, pmm_totalMemory >> 30);
	#endif

	//Liste mit reservierten Pages
	vmm_getPageTables(markPageReserved);

	SysLog("PMM", "Initialisierung abgeschlossen");
	return true;
}

/*
 * Reserviert eine Speicherstelle
 * Rückgabewert:	phys. Addresse der Speicherstelle
 * 					1 = Kein phys. Speicherplatz mehr vorhanden
 */
paddr_t pmm_Alloc()
{
	return pmm_AllocDMA(-1, 1);
}

/*
 * Gibt eine Speicherstelle frei, dabei wird wenn möglich in der Bitmap kontrolliert, ob diese schon mal freigegeben wurde
 * Params: phys. Addresse der Speicherstelle
 * Rückgabewert:	true = Aktion erfolgreich
 * 					false = Aktion fehlgeschlagen (zu wenig Speicher)
 */
void pmm_Free(paddr_t Address)
{
	page_range_t range = {
		.page_nr = Address / MM_BLOCK_SIZE,
		.size_exp = 0
	};
	LOCKED_TASK(pmm_lock, {
		insertRange(range);
		pmm_freePages++;
	});
}

//Für DMA erforderlich
paddr_t pmm_AllocDMA(paddr_t maxAddress, size_t pages)
{
	return LOCKED_RESULT(pmm_lock, {
		page_range_t range = findRange(maxAddress, pages);
		paddr_t result = 1;
		if (range.raw != 0) {
			pmm_freePages -= pages;
			result = getStartPageFromRange(range);
			size_t page_count = 1ul << range.size_exp;
			if (page_count > pages) {
				// We have to split the range
				paddr_t start_page = result + pages * MM_BLOCK_SIZE;
				page_count -= pages;
				while (page_count > 0) {
					page_range_t range = getPageRange(&start_page, &page_count);
					insertRange(range);
				}
			}
		}
		result;
	});
}

uint64_t pmm_getTotalPages()
{
	return pmm_totalPages;
}

uint64_t pmm_getFreePages()
{
	return pmm_freePages;
}
