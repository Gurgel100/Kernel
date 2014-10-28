/*
 * sound.h
 *
 *  Created on: 18.03.2014
 *      Author: pascal
 */

#ifdef BUILD_KERNEL

#ifndef SOUND_H_
#define SOUND_H_

#include "stdint.h"

void sound_Play(uint32_t freq, uint64_t sleep);

#endif /* SOUND_H_ */

#endif
