/*
 * path.h
 *
 *  Created on: 28.05.2017
 *      Author: pascal
 */

#ifndef PATH_H_
#define PATH_H_

#include <stdbool.h>
#include <stddef.h>

/**
 * \brief Returns if a path is relative or absolute
 *
 * @param path Path to be checked
 * @return true if the path is relative, false otherwise
 */
bool path_isRelative(const char *path);

/**
 * \brief Returns if a path element is valid
 *
 * @param element Element of path to be checked
 * @return true if element is a valid path element
 */
bool path_elementIsValid(const char *element);

/**
 * \brief Gets an absolute path out of path
 *
 * The second parameter must be an absolute path to the current directory which will be prepended to path if path is a relative path.
 *
 * @param path Relative or absolute path to be converted
 * @param pwd Current working directory
 * @return a newly allocated string which holds the absolute path
 */
char *path_getAbsolute(const char *path, const char *pwd);

/**
 * \brief Appends path2 to path1
 *
 * @param path1 Path
 * @param path2 Path to append
 * @return a newly allocated string which holds the result of the concatenation of path1 and path2
 */
char *path_append(const char *path1, const char *path2);


/**
 * \brief Removes the last element of path and returns the new path
 *
 * This function removes the last element which is stored in element if element is not NULL in a newly allocated string.
 * The returned string is newly allocated and holds the path to the directory in which element is contained.
 *
 * @param path Path of which the last element should be removed
 * @param element The address to store a newly allocated string holding the removed element
 * @return a newly allocated string holding the path to the directory in which the removed element is contained
 */
char *path_removeLast(const char *path, char **element);

/**
 * \brief Split the path into its elements
 * 
 * This function splits the path into its elements. The elements are returned as an array of strings with a size of \p count.
 * The \p elements array has to be freed by the caller. The array is always allocated if no error occured.
 * 
 * @param[in]   path Path of which to extract the elements
 * @param[out]  elements An array of strings containing copies of the elements. The caller is responsible for freeing it
 * @param[out]  count The number of valid elements in the \p elements array
 * 
 * @return non-zero if an error occured else 0
 */
int path_split(const char *path, char ***elements, size_t *count);

#endif /* PATH_H_ */
