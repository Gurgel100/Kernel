/*
 * version.c
 *
 *  Created on: 31.10.2017
 *      Author: pascal
 */

#include "version.h"

#define VERSION_MAJOR	0
#define VERSION_MINOR	0
#define VERSION_BUGFIX	0

#define _STR(x)	#x
#define STR(x)	_STR(x)

#if (defined(__GNUC__) || defined(__GNUG__)) && !(defined(__clang__) || defined(__INTEL_COMPILER))
#define COMPILER		"gcc " __VERSION__
#elif (defined(__clang__))
#define COMPILER		"clang " __clang_version__
#else
#define COMPILER		"unknown"
#endif

#ifndef GIT_BRANCH
#define GIT_BRANCH		unknown
#endif

#ifndef GIT_COMMIT_ID
#define GIT_COMMIT_ID	unknown
#endif

const unsigned version_major = VERSION_MAJOR;
const unsigned version_minor = VERSION_MINOR;
const unsigned version_bugfix = VERSION_BUGFIX;

const char *version_branch = STR(GIT_BRANCH);
const char *version_commitId = STR(GIT_COMMIT_ID);

const char *version_buildDate = __DATE__;
const char *version_buildTime = __TIME__;

const char *version_compiler = COMPILER;
