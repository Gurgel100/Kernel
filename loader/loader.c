/*
 * loader.c
 *
 *  Created on: 17.10.2014
 *      Author: pascal
 */


#include "loader.h"


int loader_load(FILE *fp)
{
	if(elfLoad(fp) != 0)
		return -1;
	return 0;
}
