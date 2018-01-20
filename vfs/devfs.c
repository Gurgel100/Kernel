/*
 * devfs.c
 *
 *  Created on: 07.01.2018
 *      Author: pascal
 */

#include "devfs.h"
#include "partition.h"
#include "lists.h"
#include "storage.h"
#include "scsi.h"
#include "stdlib.h"
#include "string.h"
#include "assert.h"

#define GET_BYTE(value, offset) (value >> offset) & 0xFF
#define MIN(val1, val2) ((val1 < val2) ? val1 : val2)

static list_t nodes;
static vfs_filesystem_driver_t devfs_driver;
static vfs_node_dir_t root;

static int probe(vfs_filesystem_t *fs __attribute__((unused)), vfs_file_t dev)
{
	return dev == (vfs_file_t)-1 ? 0 : 1;
}

static int mount(vfs_filesystem_t *fs, vfs_file_t dev __attribute__((unused)))
{
	fs->root = &root;
	return 0;
}

static int umount(vfs_filesystem_t *fs __attribute__((unused)))
{
	return 0;
}

static vfs_node_dir_t *get_root()
{
	return &root;
}

static int root_visitChilds(vfs_node_dir_t *node __attribute__((unused)), uint64_t start, vfs_node_dir_callback_t callback, void *context)
{
	vfs_node_dev_t *child;
	while((child = list_get(nodes, start++)) != NULL && callback(&child->base, context));
	return 0;
}

static vfs_node_t *root_findChild(vfs_node_dir_t *node __attribute__((unused)), const char *name)
{
	vfs_node_dev_t *child;
	size_t i = 0;
	while((child = list_get(nodes, i++)) != NULL)
	{
		if(strcmp(child->base.name, name) == 0)
			return &child->base;
	}

	return NULL;
}

void devfs_init()
{
	//Initialisiere Geräteliste
	nodes = list_create();

	vfs_node_dir_init(&root, "/");
	vfs_registerFilesystemDriver(&devfs_driver);
}

int devfs_registerDeviceNode(vfs_node_dev_t *node)
{
	node->base.parent = &root;
	return list_push(nodes, node) == NULL ? 1 : 0;
}

static vfs_filesystem_driver_t devfs_driver = {
	.name = "devfs",
	.info = VFS_FILESYSTEM_INFO_VIRTUAL | VFS_FILESYSTEM_INFO_MODULE,

	.probe = probe,
	.mount = mount,
	.umount = umount,
	.get_root = get_root
};

static vfs_node_dir_t root = {
	.visitChilds = root_visitChilds,
	.findChild = root_findChild
};
