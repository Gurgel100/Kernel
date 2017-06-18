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

char *path_removeLast(char *path)
{
	char *last_slash = strrchr(path, '/');
	if(last_slash == NULL)
	{
		return NULL;
	}
	else
	{
		if(*(last_slash + 1) == '\0')
		{
			*last_slash = '\0';
			if(last_slash == path)
				return path + 1;
			else
				return path_removeLast(path);
		}
		else
		{
			*last_slash = '\0';
			return last_slash + 1;
		}
	}
}
