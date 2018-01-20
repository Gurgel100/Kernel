/*
 * node.h
 *
 *  Created on: 14.08.2017
 *      Author: pascal
 */

#ifndef NODE_H_
#define NODE_H_

#include "lock.h"
#include "list.h"
#include "hashmap.h"
#include "stdint.h"
#include <bits/sys_types.h>

typedef enum{
	VFS_NODE_DIR, VFS_NODE_FILE, VFS_NODE_LINK, VFS_NODE_DEV
}vfs_node_type_t;

struct vfs_node_dir;

typedef struct vfs_node{
	/**
	 * Type of node
	 */
	vfs_node_type_t type;

	/**
	 * This field indicates if the node is momentarily locked.
	 * If the node is locked then no operation can be done on this node and all depending nodes.
	 */
	lock_t lock;

	/**
	 * The name of the node
	 */
	const char *name;

	/**
	 * The parent node
	 */
	struct vfs_node_dir *parent;

	/**
	 * All the streams which are opened on this node
	 */
	hashmap_t *streams;

	/**
	 * \brief Rename node
	 *
	 * @param node Node which should be renamed. Should be the same as the object this function is called on.
	 * @param name New name of the node
	 */
	int (*rename)(struct vfs_node *node, const char *name);

	/**
	 * \brief Move node
	 *
	 * This function can only be called if the node is moved within the same filesystem. Otherwise this function may fail.
	 * @param node Node which should be moved. Should be the same as the object this function is called on.
	 * @param dest Destination node to where the node should be moved
	 */
	int (*move)(struct vfs_node *node, struct vfs_node *dest);

	/**
	 * \brief Delete node
	 *
	 * @param node Node which should be deleted. Should be the same as the object this function is called on.
	 */
	int (*delete)(struct vfs_node *node);

	/**
	 * Get attributes of node @node
	 *
	 * @param node Node
	 * @param attribute Attribute which should be received
	 * @param value Non-null pointer to a location where the value should be written
	 */
	int (*getAttribute)(struct vfs_node *node, vfs_fileinfo_t attribute, uint64_t *value);

	/**
	 * Set attributes of node @node
	 *
	 * @param node Node
	 * @param attribute Attribute which should be set
	 * @param value The value which should be written
	 */
	int (*setAttribute)(struct vfs_node *node, vfs_fileinfo_t attribute, uint64_t value);

	/**
	 * Function which is called to destroy this node.
	 * Does not affect any data on a persistent drive.
	 */
	void (*destroy)(struct vfs_node *node);
}vfs_node_t;

typedef bool (*vfs_node_dir_callback_t)(vfs_node_t *child, void *context);

typedef struct vfs_node_dir{
	vfs_node_t base;

	/**
	 * A list of mounted filesystems. Only the top one is active.
	 */
	list_t mounts;

	/**
	 * \brief Visits all childs of the directory and calls the callback
	 *
	 * @param node Directory node of which the childs should be visited. Should be the same as the object this function is called on.
	 * @param start Offset from where to start calling the callback
	 * @param callback The callback function which will be called for each child. Returns false when the visiting should be finished.
	 * @param context A context which is passed to the callback function
	 * @return !0 if error
	 */
	int (*visitChilds)(struct vfs_node_dir *node, uint64_t start, vfs_node_dir_callback_t callback, void *context);

	/**
	 * \brief Finds a child with the specified name
	 *
	 * @param node Directory node from where the child should be searched. Should be the same as the object this function is called on.
	 * @param name The name of the child
	 * @return The child with the specified name or NULL if there is no child with the specified name
	 */
	vfs_node_t *(*findChild)(struct vfs_node_dir *node, const char *name);

	/**
	 * \brief Creates a child with the specified type and name
	 *
	 * @param node Directory node where the child should be created. Should be the same as the object this function is called on.
	 * @param type The type of the child
	 * @param name The name of the child
	 * @return !0 if error
	 */
	int (*createChild)(struct vfs_node_dir *node, vfs_node_type_t type, const char *name);
}vfs_node_dir_t;

typedef struct vfs_node_file{
	vfs_node_t base;

	/**
	 * \brief Read from the file
	 *
	 * @param node File node from which should be read. Should be the same as the object this function is called on.
	 * @param start Byte offset from where to start the read
	 * @param size Number of bytes to be read
	 * @param buffer The buffer where the data should be written
	 * @return Number of bytes read
	 */
	size_t (*read)(struct vfs_node_file *node, uint64_t start, size_t size, void *buffer);

	/**
	 * \brief Write to the file
	 *
	 * @param node File node to which should be written. Should be the same as the object this function is called on.
	 * @param start Byte offset from where to start the write
	 * @param size Number of bytes to be written
	 * @param buffer The buffer from where the data should be read
	 * @return Number of bytes written
	 */
	size_t (*write)(struct vfs_node_file *node, uint64_t start, size_t size, const void *buffer);

	/**
	 * \brief Truncate the file
	 *
	 * @param node File node which should be truncated. Should be the same as the object this function is called on.
	 * @param size The number of bytes to which the file should be truncated
	 * @return !0 if error
	 */
	int (*truncate)(struct vfs_node_file *node, size_t size);
}vfs_node_file_t;

typedef struct vfs_node_link{
	vfs_node_t base;

	/**
	 * \brief Get the linked node from the link
	 *
	 * @param node Link node from which the linked node should be received. Should be the same as the object this function is called on.
	 * @return Linked node or NULL if error
	 */
	vfs_node_t *(*getLink)(struct vfs_node_link *node);

	/**
	 * \brief Set the node to which the link should be linking
	 *
	 * @param node Link node which should link to the linked node. Should be the same as the object this function is called on.
	 * @param link Node to which should be linked
	 * @return !0 if error
	 */
	int (*setLink)(struct vfs_node_link *node, vfs_node_t *link);
}vfs_node_link_t;

/**
 * @brief Type of device
 */
typedef enum{
	/**
	 * Storage device
	 */
	VFS_DEVICE_STORAGE		= 1 << 0,

	/**
	 * Character device
	 */
	VFS_DEVICE_CHARACTER	= 1 << 1,

	/**
	 * Virtual device
	 */
	VFS_DEVICE_VIRTUAL		= 1 << 3,

	/**
	 * Mountable
	 */
	VFS_DEVICE_MOUNTABLE	= 1 << 4,
}vfs_device_type_t;

//forward declaration
typedef struct vfs_filesystem vfs_filesystem_t;

typedef struct vfs_node_dev{
	vfs_node_t base;

	/**
	 * The type of the device
	 */
	vfs_device_type_t type;

	/**
	 * If this field is not NULL then this device is already mounted by the filesystem to which it points to.
	 * It can only be mounted by this filesystem otherwise an error should occur.
	 */
	vfs_filesystem_t *fs;

	/**
	 * \brief Read from the device
	 *
	 * @param node Device node from which should be read. Should be the same as the object this function is called on.
	 * @param start Byte offset from where to start the read
	 * @param size Number of bytes to be read
	 * @param buffer The buffer where the data should be written
	 * @return Number of bytes read
	 */
	size_t (*read)(struct vfs_node_dev *node, uint64_t start, size_t size, void *buffer);

	/**
	 * \brief Write to the device
	 *
	 * @param node Device node to which should be written. Should be the same as the object this function is called on.
	 * @param start Byte offset from where to start the write
	 * @param size Number of bytes to be written
	 * @param buffer The buffer from where the data should be read
	 * @return Number of bytes written
	 */
	size_t (*write)(struct vfs_node_dev *node, uint64_t start, size_t size, const void *buffer);
}vfs_node_dev_t;

int vfs_node_init(vfs_node_t *node, const char *name);
void vfs_node_deinit(vfs_node_t *node);

int vfs_node_dir_init(vfs_node_dir_t *node, const char *name);
void vfs_node_dir_deinit(vfs_node_dir_t *node);

int vfs_node_file_init(vfs_node_file_t *node, const char *name);
void vfs_node_file_deinit(vfs_node_file_t *node);

int vfs_node_link_init(vfs_node_link_t *node, const char *name);
void vfs_node_link_deinit(vfs_node_link_t *node);

int vfs_node_dev_init(vfs_node_dev_t *node, const char *name);
void vfs_node_dev_deinit(vfs_node_dev_t *node);

#endif /* NODE_H_ */
