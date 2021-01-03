/*
 * dirent.h
 *
 *  Created on: 21.03.2017
 *      Author: pascal
 */

#ifndef DIRENT_H_
#define DIRENT_H_

#include <sys/types.h>

//Typen für d_type
typedef enum{
	DT_UNKNOWN, DT_DIR, DT_FILE, DT_LINK, DT_DEV
}d_type_t;

struct dirent{
	ino_t d_ino;
	off_t d_off;
	unsigned short d_reclen;
	d_type_t d_type;
	char d_name[];
};

typedef struct dirstream DIR;

int alphasort(const struct dirent **d1, const struct dirent **d2);
int closedir(DIR* dirp);
DIR* opendir(const char* dirname);
struct dirent* readdir(DIR* dirp);
int readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result);
void rewinddir(DIR* dirp);
int scandir(const char *dir, struct dirent ***namelist,
		int (*sel)(const struct dirent*),
		int (*compar)(const struct dirent**, const struct dirent**));
void seekdir(DIR* dirp, long loc);
long telldir(DIR* dirp);

#endif /* DIRENT_H_ */
