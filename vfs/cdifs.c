/*
 * cdifs.c
 *
 *  Created on: 07.01.2018
 *      Author: pascal
 */

#include "cdifs.h"
#include "vfs.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <hashmap.h>
#include <hash_helpers.h>

#define CAST_NODE(cdifs_type, node)	((cdifs_type*)((void*)node - offsetof(cdifs_type, vfs_node)))

typedef struct{
	struct cdi_fs_stream stream;
}cdifs_vfs_node_t;

typedef struct{
	vfs_filesystem_driver_t base;

	struct cdi_fs_driver *driver;
}cdifs_vfs_filesystem_driver_t;

typedef struct{
	cdifs_vfs_node_t base;
	vfs_node_dir_t vfs_node;

	hashmap_t *childs;
	bool childs_loaded;
}cdifs_vfs_node_dir_t;

typedef struct{
	cdifs_vfs_node_t base;
	vfs_node_file_t vfs_node;
}cdifs_vfs_node_file_t;

typedef struct{
	cdifs_vfs_node_t base;
	vfs_node_link_t vfs_node;
}cdifs_vfs_node_link_t;

typedef struct{
	vfs_node_dir_callback_t callback;
	void *context;
	size_t start;
	bool stop;
}cdifs_visit_childs_context_t;

static vfs_node_t *createNode(cdifs_vfs_node_dir_t *parent, struct cdi_fs_stream *stream);

static int loadRes(struct cdi_fs_stream *stream)
{
	if(!stream->res->loaded)
	{
		return stream->res->res->load(stream) == 1 ? 0 : 1;
	}
	return 0;
}

static int loadChilds(cdifs_vfs_node_dir_t *node)
{
	if(!node->childs_loaded)
	{
		struct cdi_fs_stream *stream = &node->base.stream;
		struct cdi_fs_stream child_stream = {
			.fs = stream->fs
		};

		size_t i = 0;
		cdi_list_t childs = stream->res->dir->list(stream);
		while((child_stream.res = cdi_list_get(childs, i++)) != NULL)
		{
			vfs_node_t *child = createNode(node, &child_stream);
			if(child == NULL)
				return 1;
			hashmap_set(node->childs, child->name, child);
		}
		node->childs_loaded = true;
	}
	return 0;
}

static void visitChilds(const void *key __attribute__((unused)), const void *obj, void *context)
{
	cdifs_visit_childs_context_t *c = context;
	vfs_node_t *node = (vfs_node_t*)obj;

	if(c->start > 0)
		c->start--;
	else if(!c->stop)
		c->stop = c->callback(node, c->context);
}

static int dir_visitChilds(vfs_node_dir_t *node, uint64_t start, vfs_node_dir_callback_t callback, void *context)
{
	cdifs_vfs_node_dir_t *cdi_node = CAST_NODE(cdifs_vfs_node_dir_t, node);

	cdifs_visit_childs_context_t visitContext = {
		.callback = callback,
		.context = context,
		.start = start
	};

	return LOCKED_RESULT(node->base.lock, {
		int status = loadChilds(cdi_node);
		if(!status) {
			hashmap_visit(cdi_node->childs, visitChilds, &visitContext);
		}
		status;
	});
}

static vfs_node_t *dir_findChild(vfs_node_dir_t *node, const char *name)
{
	cdifs_vfs_node_dir_t *cdi_node = CAST_NODE(cdifs_vfs_node_dir_t, node);
	vfs_node_t *child = NULL;

	LOCKED_TASK(node->base.lock, {
		loadChilds(cdi_node);
		hashmap_search(cdi_node->childs, name, (void**)&child);
	});

	return child;
}

static int dir_createChild(vfs_node_dir_t *node, vfs_node_type_t type, const char *name)
{
	cdifs_vfs_node_dir_t *cdi_node = CAST_NODE(cdifs_vfs_node_dir_t, node);
	struct cdi_fs_stream *stream = &cdi_node->base.stream;

	lock_node_t lock_node;
	lock(&node->base.lock, &lock_node);

	loadRes(&cdi_node->base.stream);

	if(!stream->res->flags.create_child)
	{
		unlock(&node->base.lock, &lock_node);
		return 1;
	}

	cdi_fs_res_class_t res_class;
	switch(type)
	{
		case VFS_NODE_FILE:
			res_class = CDI_FS_CLASS_FILE;
			break;
		case VFS_NODE_DIR:
			res_class = CDI_FS_CLASS_DIR;
			break;
		case VFS_NODE_LINK:
			res_class = CDI_FS_CLASS_LINK;
			break;
		default:
			unlock(&node->base.lock, &lock_node);
			return 1;
	}

	struct cdi_fs_stream newStream = {
		.fs = stream->fs
	};

	if(!stream->res->dir->create_child(&newStream, name, stream->res))
	{
		unlock(&node->base.lock, &lock_node);
		return -1;
	}

	if(!stream->res->res->assign_class(&newStream, res_class))
	{
		stream->res->res->remove(&newStream);
		unlock(&node->base.lock, &lock_node);
		return -1;
	}

	if(cdi_node->childs_loaded)
	{
		vfs_node_t *child = createNode(cdi_node, &newStream);
		if(child == NULL)
			cdi_node->childs_loaded = false;
		else
			hashmap_set(cdi_node->childs, child->name, child);
	}

	unlock(&node->base.lock, &lock_node);

	return 0;
}

static size_t file_read(vfs_node_file_t *node, uint64_t start, size_t size, void *buffer)
{
	cdifs_vfs_node_file_t *cdi_node = CAST_NODE(cdifs_vfs_node_file_t, node);

	size_t filesize = cdi_node->base.stream.res->res->meta_read(&cdi_node->base.stream, CDI_FS_META_SIZE);
	if(start > filesize)
		start = filesize;
	if(start + size > filesize)
		size = filesize - start;
	return cdi_node->base.stream.res->file->read(&cdi_node->base.stream, start, size, buffer);
}

static size_t file_write(vfs_node_file_t *node, uint64_t start, size_t size, const void *buffer)
{
	cdifs_vfs_node_file_t *cdi_node = CAST_NODE(cdifs_vfs_node_file_t, node);

	return cdi_node->base.stream.res->file->write(&cdi_node->base.stream, start, size, buffer);
}

static int file_truncate(vfs_node_file_t *node, size_t size)
{
	cdifs_vfs_node_file_t *cdi_node = CAST_NODE(cdifs_vfs_node_file_t, node);

	return cdi_node->base.stream.res->file->truncate(&cdi_node->base.stream, size) == 1 ? 0 : 1;
}

static int getAttribute(vfs_node_t *node, vfs_fileinfo_t attribute, uint64_t *value)
{
	assert(offsetof(cdifs_vfs_node_dir_t, vfs_node) == offsetof(cdifs_vfs_node_file_t, vfs_node));
	assert(offsetof(cdifs_vfs_node_dir_t, vfs_node) == offsetof(cdifs_vfs_node_link_t, vfs_node));

	cdifs_vfs_node_t *cdi_node = &CAST_NODE(cdifs_vfs_node_dir_t, node)->base;
	switch(attribute)
	{
		case VFS_INFO_FILESIZE:
			*value = cdi_node->stream.res->res->meta_read(&cdi_node->stream, CDI_FS_META_SIZE);
			break;
		case VFS_INFO_BLOCKSIZE:
			*value = cdi_node->stream.res->res->meta_read(&cdi_node->stream, CDI_FS_META_BLOCKSZ);
			break;
		default:
			return 1;
	}
	return 0;
}

static void destroyNode(const void *obj)
{
	vfs_node_t *node = (vfs_node_t*)obj;
	node->destroy(node);
}

static vfs_node_t *createNode(cdifs_vfs_node_dir_t *parent, struct cdi_fs_stream *stream)
{
	if(loadRes(stream))
		return NULL;

	vfs_node_t *n = NULL;
	if(stream->res->dir != NULL)
	{
		cdifs_vfs_node_dir_t *node = calloc(1, sizeof(cdifs_vfs_node_dir_t));
		if(vfs_node_dir_init(&node->vfs_node, stream->res->name))
			return NULL;
		node->base.stream = *stream;
		node->childs = hashmap_create(string_hash, string_hash, string_equal, NULL, destroyNode, NULL, NULL, 0);
		node->vfs_node.visitChilds = dir_visitChilds;
		node->vfs_node.findChild = dir_findChild;
		node->vfs_node.createChild = dir_createChild;

		n = &node->vfs_node.base;
	}
	else if(stream->res->file != NULL)
	{
		cdifs_vfs_node_file_t *node = calloc(1, sizeof(cdifs_vfs_node_file_t));
		if(vfs_node_file_init(&node->vfs_node, stream->res->name))
			return NULL;
		node->base.stream = *stream;
		node->vfs_node.read = file_read;
		node->vfs_node.write = file_write;
		node->vfs_node.truncate = file_truncate;

		n = &node->vfs_node.base;
	}
	else if(stream->res->link != NULL)
	{
		cdifs_vfs_node_link_t *node = calloc(1, sizeof(cdifs_vfs_node_link_t));
		if(vfs_node_link_init(&node->vfs_node, stream->res->name))
			return NULL;
		node->base.stream = *stream;

		n = &node->vfs_node.base;
	}

	n->parent = &parent->vfs_node;
	n->getAttribute = getAttribute;

	return n;
}

static int probe(vfs_filesystem_t *fs, vfs_stream_t *dev)
{
	cdifs_vfs_filesystem_driver_t *driver = (cdifs_vfs_filesystem_driver_t*)fs->driver;
	struct cdi_fs_filesystem cdi_fs = {
		.driver = driver->driver,
		.osdep = {.fp = dev},
	};
	char *volname = NULL;
	int res = driver->driver->fs_probe(&cdi_fs, &volname);
	free(volname);
	return 1 - res;
}

static int mount(vfs_filesystem_t *fs, vfs_stream_t *dev)
{
	cdifs_vfs_filesystem_driver_t *driver = (cdifs_vfs_filesystem_driver_t*)fs->driver;
	struct cdi_fs_filesystem *cdi_fs = calloc(1, sizeof(struct cdi_fs_filesystem));
	if(cdi_fs == NULL)
		return 1;

	cdi_fs->driver = driver->driver;
	cdi_fs->osdep.fp = dev;
	fs->opaque = cdi_fs;

	if(!driver->driver->fs_init(cdi_fs))
		return 1;

	struct cdi_fs_stream stream = {
		.fs = cdi_fs,
		.res = cdi_fs->root_res
	};
	fs->root = (vfs_node_dir_t*)createNode(NULL, &stream);
	assert(fs->root->base.type == VFS_NODE_DIR);

	return 0;
}

static int umount(vfs_filesystem_t *fs)
{
	cdifs_vfs_filesystem_driver_t *driver = (cdifs_vfs_filesystem_driver_t*)fs->driver;
	struct cdi_fs_filesystem *cdi_fs = fs->opaque;
	int res = driver->driver->fs_destroy(cdi_fs);
	free(cdi_fs);
	return 1 - res;
}

int cdifs_registerFilesystemDriver(struct cdi_fs_driver *driver)
{
	cdifs_vfs_filesystem_driver_t *drv = calloc(1, sizeof(cdifs_vfs_filesystem_driver_t));
	if(drv == NULL)
		return 1;

	drv->base.name = driver->drv.name;
	drv->base.probe = probe;
	drv->base.mount = mount;
	drv->base.umount = umount;
	drv->driver = driver;

	return vfs_registerFilesystemDriver(&drv->base);
}
