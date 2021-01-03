/*
 * node.c
 *
 *  Created on: 15.10.2017
 *      Author: pascal
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vfs/node.h>

static uint64_t mode_hash(const void *key, __attribute__((unused)) void *context)
{
	const vfs_mode_t *mode = key;
	return (uint64_t)*mode;
}

static bool mode_equal(const void *a, const void *b, __attribute__((unused)) void *context)
{
	const vfs_mode_t *modea = a;
	const vfs_mode_t *modeb = b;
	return *modea == *modeb;
}

/*
 * Erstelle eine simple Node. Setzt nicht den Typ der Node und nicht die abhängigen Daten.
 * Parameter:	parent = Node zu welcher die neue Node hinzugefügt werden soll
 * 				name = Name der Node
 * Rückgabe:	Pointer zur neuen Node
 */
int vfs_node_init(vfs_node_t *node, const char *name)
{
	node->name = strdup(name);
	node->lock = LOCK_INIT;
	node->parent = NULL;
	node->streams = hashmap_create(mode_hash, mode_hash, mode_equal, NULL, NULL, NULL, NULL, 0);
	if(node->streams == NULL)
	{
		free((char*)node->name);
		return 1;
	}

	return 0;
}

/*
 * Löscht eine Node. Löscht keine Kinder.
 * Parameter:	node = Node die gelöscht werden soll
 */
void vfs_node_deinit(vfs_node_t *node)
{
	assert(node != NULL);

	lock_node_t lock_node;
	lock(&node->lock, &lock_node);
	free((char*)node->name);
	hashmap_destroy(node->streams);
	free(node);
}

int vfs_node_dir_init(vfs_node_dir_t *node, const char *name)
{
	int res = vfs_node_init(&node->base, name);
	if(res)
		return res;
	node->base.type = VFS_NODE_DIR;

	node->mounts = list_create();
	if(node->mounts == NULL)
	{
		vfs_node_deinit(&node->base);
		return 1;
	}

	return 0;
}

void vfs_node_dir_deinit(vfs_node_dir_t *node)
{
	list_destroy(node->mounts);
	vfs_node_deinit(&node->base);
}

int vfs_node_file_init(vfs_node_file_t *node, const char *name)
{
	int res = vfs_node_init(&node->base, name);
	if(res)
		return res;
	node->base.type = VFS_NODE_FILE;

	return 0;
}

void vfs_node_file_deinit(vfs_node_file_t *node)
{
	vfs_node_deinit(&node->base);
}

int vfs_node_link_init(vfs_node_link_t *node, const char *name)
{
	int res = vfs_node_init(&node->base, name);
	if(res)
		return res;
	node->base.type = VFS_NODE_LINK;

	return 0;
}

void vfs_node_link_deinit(vfs_node_link_t *node)
{
	vfs_node_deinit(&node->base);
}

int vfs_node_dev_init(vfs_node_dev_t *node, const char *name)
{
	int res = vfs_node_init(&node->base, name);
	if(res)
		return res;
	node->base.type = VFS_NODE_DEV;

	return 0;
}

void vfs_node_dev_deinit(vfs_node_dev_t *node)
{
	vfs_node_deinit(&node->base);
}
