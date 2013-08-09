/*
 * \brief  Audio handling
 * \author Sebastian Sumpf
 * \date   2012-11-20
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__AUDIO_H_
#define _INCLUDE__AUDIO_H_

/**
 * Initialize audio if device is present
 */
int audio_init();

/**
 * Play data of size
 */
int audio_play(short *data, unsigned size);

#endif /* _INCLUDE__AUDIO_H_ */
