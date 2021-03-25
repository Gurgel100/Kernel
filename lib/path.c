/*
 * paths.c
 *
 *  Created on: 28.05.2017
 *      Author: pascal
 */

#include "assert.h"
#include "string.h"
#include "stdlib.h"
#include "path.h"

bool path_isRelative(const char *path)
{
	assert(path != NULL);

	if(path[0] == '/')
		return false;
	return true;
}

bool path_elementIsValid(const char *element)
{
	return strchr(element, '/') == NULL;
}

char *path_getAbsolute(const char *path, const char *pwd)
{
	if(path == NULL)
		return NULL;

	if(path_isRelative(path))
		return path_append(pwd, path);
	else
		return strdup(path);
}

char *path_append(const char *path1, const char *path2)
{
	size_t path1_len = strlen(path1);
	size_t path2_len = strlen(path2);
	char *res_path = malloc(path1_len + path2_len + 1);

	if(res_path == NULL)
		return NULL;

	strcpy(res_path, path1);
	switch((path1[path1_len - 1] == '/') + (path2[0] == '/'))
	{
		case 2:
			memcpy(res_path + path1_len, path2 + 1, path2_len);
		break;
		case 1:
			memcpy(res_path + path1_len, path2, path2_len + 1);
		break;
		default:
			res_path[path1_len] = '/';
			memcpy(res_path + path1_len + 1, path2, path2_len + 1);
	}
	return res_path;
}

char *path_removeLast(const char *path, char **element)
{
	char *new_path = strdup(path);
	char *last_slash = strrchr(new_path, '/');
	if(last_slash == NULL)
	{
		if(element != NULL)
			*element = strdup(new_path);
		*new_path = '\0';
	}
	else
	{
		if(*(last_slash + 1) == '\0')
		{
			if(last_slash == new_path)
			{
				if(element != NULL)
					*element = strdup(new_path + 1);
			}
			else
			{
				*last_slash = '\0';
				char *returned_path = path_removeLast(new_path, element);
				free(new_path);
				new_path = returned_path;
			}
		}
		else
		{
			*last_slash = '\0';
			if(element != NULL)
				*element = strdup(last_slash + 1);
		}
	}
	return new_path;
}

int path_split(const char *path, char ***elements, size_t *count) {
	if (path == NULL) return -1;

	size_t element_count = 0;
	size_t allocated_elements = 2;
	const char *current_element = path;
	*elements = malloc(allocated_elements * sizeof(*elements));
	if (*elements == NULL) return -1;

	while (*current_element != '\0') {
		char *element_end = strchr(current_element, '/');
		bool last_element = false;
		if (element_end == NULL) {
			element_end = strchr(current_element, '\0');
			last_element = true;
		}
		if (element_count + 1 > allocated_elements) {
			allocated_elements *= 2;
			char **new_elements = realloc(*elements, allocated_elements * sizeof(*elements));
			if (new_elements == NULL) goto error;
			*elements = new_elements;
		}

		size_t element_len = element_end - current_element;

		// Skip empty elements
		if (element_len != 0) {
			(*elements)[element_count] = malloc(element_len + 1);
			if ((*elements)[element_count] == NULL) goto error;
			memcpy((*elements)[element_count], current_element, element_len);
			(*elements)[element_count][element_len] = '\0';
			element_count++;
		}
		current_element = element_end + !last_element;
	}
	*count = element_count;
	return 0;

error:
	for (size_t i = 0; i <= element_count; i++) {
		free((*elements)[i]);
	}
	free(*elements);
	return -1;
}
