/*
 * \brief  Genode-specific audio backend
 * \author Christian Prochaska
 * \date   2012-03-13
 *
 * based on the dummy SDL audio driver
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "SDL_config.h"

#ifndef _SDL_genodeaudio_h
#define _SDL_genodeaudio_h

#include "SDL_sysaudio.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *_this

#endif /* _SDL_genodeaudio_h */
