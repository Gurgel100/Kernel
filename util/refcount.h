/*
 * refcount.h
 *
 *  Created on: 21.03.2017
 *      Author: pascal
 */
/**
 * \file
 * Contains functions for reference counting
 */

#ifndef REFCOUNT_H_
#define REFCOUNT_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

//Name of the reference count field
#define REFCOUNT_FIELD_NAME				__refcount

/**
 * \brief Generates a field needed for reference counting.
 *
 * This macro generates a structure field which is needed when using reference counting.
 */
#define REFCOUNT_FIELD					refcount_t REFCOUNT_FIELD_NAME

/**
 * \brief Initializes the reference counting of an object.
 *
 * \param obj The reference counted object
 * \param free Function which will be called when the object should be freed
 */
#define REFCOUNT_INIT(obj, free)	refcount_init(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME), free)

/**
 * \brief Retains a reference counted object.
 *
 * \param obj Reference counted object
 * \return NULL if the object could not be retained otherwise \p obj
 */
#define REFCOUNT_RETAIN(obj)		refcount_retain(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME))

/**
 * \brief Releases a reference counted object.
 *
 * \param obj Reference counted object
 * \return true if object was freed false otherwise
 */
#define REFCOUNT_RELEASE(obj)		refcount_release(obj, offsetof(typeof(*obj), REFCOUNT_FIELD_NAME))

/**
 * \brief Structure holding reference counting information.
 *
 * This structure is not intended to be used directly in another structure or union. Use the macro #REFCOUNT_FIELD instead.
 */
typedef struct{
	uint64_t ref_count;
	void (*free)(const void*);
}refcount_t;

/**
 * \brief Initializes the reference counting of an object.
 *
 * This function is not intended for direct use. Use the macro #REFCOUNT_INIT instead.
 * \param obj The reference counted object
 * \param offset Offset in the object of the reference count structure
 * \param free Function which will be called when the object should be freed
 */
void refcount_init(void *obj, size_t offset, void (*free)(const void*));

/**
 * \brief Retains a reference counted object.
 *
 * This function is not intended for direct use. Use the macro #REFCOUNT_RETAIN instead.
 * \param obj Reference counted object
 * \param offset Offset in the object of the reference count structure
 * \return NULL if the object could not be retained otherwise \p obj
 */
void *refcount_retain(void *obj, size_t offset);

/**
 * \brief Releases a reference counted object.
 *
 * This function is not intended for direct use. Use the macro #REFCOUNT_RELEASE instead.
 * \param obj Reference counted object
 * \param offset Offset in the object of the reference count structure
 * \return true if object was freed false otherwise
 */
bool refcount_release(void *obj, size_t offset);

#endif /* REFCOUNT_H_ */
