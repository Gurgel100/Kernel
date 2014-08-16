/*
 * cache.c
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#include "cache.h"
#include "stdbool.h"

typedef struct{
		struct cdi_cache_block block;

		bool dirty;

		size_t ref_count;
}block_t;

typedef struct{
		struct cdi_cache cache;

		size_t private_len;
		size_t block_count;
		size_t block_used;

		//Liste aller Blöcke
		cdi_list_t *blocks;

		/** Callback zum Lesen eines Blocks */
		cdi_cache_read_block_t* read_block;

		/** Callback zum Schreiben eines Blocks */
		cdi_cache_write_block_t* write_block;

		//Letzter Parameter für die Callbacks
		void *prv_data;
}cache_t;

/**
 * Cache erstellen
 *
 * @param block_size    Groesse der Blocks die der Cache verwalten soll
 * @param blkpriv_len   Groesse der privaten Daten die fuer jeden Block
 *                      alloziert werden und danach vom aurfrufer frei benutzt
 *                      werden duerfen
 * @param read_block    Funktionspointer auf eine Funktion zum einlesen eines
 *                      Blocks.
 * @param write_block   Funktionspointer auf eine Funktion zum schreiben einses
 *                      Blocks.
 * @param prv_data      Wird den Callbacks als letzter Parameter uebergeben
 *
 * @return Pointer auf das Cache-Handle
 */
struct cdi_cache* cdi_cache_create(size_t block_size, size_t blkpriv_len,
    cdi_cache_read_block_t* read_block,
    cdi_cache_write_block_t* write_block,
    void* prv_data)
{
		cache_t *cache;
		cache = malloc(sizeof(*cache));

		cache->cache.block_size = block_size;
		cache->prv_data = prv_data;
		cache->private_len = blkpriv_len;
		cache->read_block = read_block;
		cache->write_block = write_block;

		cache->block_count = 256;
		cache->block_used = 0;

		return (struct cdi_cache*)cache;
}

/**
 * Cache zerstoeren
 *///TODO
void cdi_cache_destroy(struct cdi_cache* cache)
{
	cache_t *c;
	c = (cache_t*)cache;

	//Erst reservierte Blocks freigeben
	if(c->block_used)
	{
		size_t i;
		for(i = 0; i < c->block_used; i++)
		{
		}
	}

	free(c);
}

/**
 * Cache-Block holen. Dabei wird intern ein Referenzzaehler erhoeht, sodass der
 * Block nicht freigegeben wird, solange er benutzt wird. Das heisst aber auch,
 * dass der Block nach der Benutzung wieder freigegeben werden muss, da sonst
 * die freien Blocks ausgehen.
 *
 * @param cache     Cache-Handle
 * @param blocknum  Blocknummer
 * @param noread    Wenn != 0 wird der Block nicht eingelesen falls er noch
 *                  nicht im Speicher ist. Kann benutzt werden, wenn der Block
 *                  vollstaendig ueberschrieben werden soll.
 *
 * @return Pointer auf das Block-Handle oder NULL im Fehlerfall
 */
struct cdi_cache_block* cdi_cache_block_get(struct cdi_cache* cache,
    uint64_t blocknum, int noread)
{
	cache_t *c;
	void *buffer;
	c = (cache_t*)cache;

	return NULL;
}

/**
 * Cache-Block freigeben
 *
 * @param cache Cache-Handle
 * @param block Block-Handle
 */
void cdi_cache_block_release(struct cdi_cache* cache,
    struct cdi_cache_block* block)
{
}
