/*
 * unistd.h
 *
 *  Created on: 18.07.2015
 *      Author: pascal
 */

#ifndef UNISTD_H_
#define UNISTD_H_

#include <sys/types.h>

#ifndef NULL
#define NULL	(void*)0
#endif

//Konstanten für access()
#define F_OK	1	//Test for existence of file.
#define R_OK	2	//Test for read permission.
#define W_OK	4	//Test for write permission.
#define X_OK	8	//Test for execute (search) permission.

#ifndef SEEK_SET
#define SEEK_SET 1
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 2
#endif
#ifndef SEEK_END
#define SEEK_END 3
#endif

#define STDERR_FILENO	2	//File number of stderr.
#define STDIN_FILENO	0	//File number of stdin.
#define STDOUT_FILENO	1	//File number of stdout.

unsigned sleep(unsigned seconds);

#endif /* UNISTD_H_ */
