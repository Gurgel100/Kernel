/*
 * stdbool.h
 *
 *  Created on: 26.07.2012
 *      Author: pascal
 */

#ifndef STDBOOL_H_
#define STDBOOL_H_

#ifndef __bool_true_false_are_defined
	#ifndef _cplusplus
		typedef _Bool bool;
		#define true  1
		#define false 0
	#endif
	#define __bool_true_false_are_defined 1
#endif

#endif /* STDBOOL_H_ */
