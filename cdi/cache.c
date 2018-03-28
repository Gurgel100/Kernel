/*
 * cache.c
 *
 *  Created on: 13.08.2013
 *      Author: pascal
 */

#include "cache.h"
#include "stdbool.h"
#include "lists.h"
#include "stdlib.h"

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
		cdi_list_t blocks;

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
		cache->blocks = cdi_list_create();

		return (struct cdi_cache*)cache;
}

/**
 * Cache zerstoeren
 */
void cdi_cache_destroy(struct cdi_cache* cache)
{
	cache_t *c;
	c = (cache_t*)cache;

	cdi_cache_sync(cache);

	//Erst reservierte Blocks freigeben
	while(c->block_used)
	{
		block_t *b = (block_t*)cdi_list_pop(c->blocks);
		free(b->block.data);
		free(b->block.private);
		free(b);
		c->block_used--;
	}

	cdi_list_destroy(c->blocks);
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
	block_t *b;
	c = (cache_t*)cache;

	//Erst suchen, ob er nicht schon vorhanden ist
	size_t i = 0;
	while((b = cdi_list_get(c->blocks, i++)))
	{
		if(b->block.number == blocknum)
			goto end;
	}

	if(c->block_used < c->block_count)
	{
		//Neuen Block in Cache legen
		b = calloc(1, sizeof(*b));
		b->block.data = malloc(c->cache.block_size);
		b->block.private = malloc(c->private_len);
		b->block.number = blocknum;
		cdi_list_push(c->blocks, b);
		c->block_used++;
	}
	else
	{
		//Nach einem Block suchen, der nicht mehr verwendet wird
		size_t i = 0;
		while((b = cdi_list_get(c->blocks, i++)))
		{
			if(!b->ref_count)
				break;
		}
		if(b == NULL)
			return NULL;

		if(b->dirty)
			cdi_cache_sync(cache);
		b->block.number = blocknum;
	}

	//Block einlesen, wenn nötig
	if(!noread)
	{
		if(!c->read_block(cache, blocknum, 1, b->block.data, c->prv_data))
		{
			//Fehler: Cacheblock wieder freigeben
			block_t *tmp;
			size_t i = 0;
			while((tmp = cdi_list_get(c->blocks, i)))
			{
				if(tmp == b)
				{
					free(b->block.data);
					free(b->block.private);
					free(b);
					cdi_list_remove(c->blocks, i);
					c->block_used--;
					return NULL;
				}
				i++;
			}
		}
	}

	end:
	b->ref_count++;

	return &b->block;
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
	block_t *b = (block_t*)block;
	b->ref_count--;
}

/**
 * Veraenderte Cache-Blocks auf die Platte schreiben
 *
 * @return 1 bei Erfolg, 0 im Fehlerfall
 */
int cdi_cache_sync(struct cdi_cache* cache)
{
	cache_t *c = (cache_t*)cache;
	block_t *b;
	size_t i = 0;

	while((b = (block_t*)cdi_list_get(c->blocks, i++)))
	{
		if(b->dirty)
		{
			if(!c->write_block(cache, b->block.number, 1, b->block.data, c->prv_data))
				return 0;
			b->dirty = false;
		}
	}
	return 1;
}

/**
 * Cache-Block als veraendert markieren
 *
 * @param cache Cache-Handle
 * @param block Block-Handle
 */
void cdi_cache_block_dirty(struct cdi_cache* cache, struct cdi_cache_block* block)
{
	block_t *b = (block_t*)block;
	b->dirty = true;
}
