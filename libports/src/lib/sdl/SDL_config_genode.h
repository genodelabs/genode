/*
 * \brief  Genode specific defines
 * \author Stefan Kalkowski
 * \date   2008-12-12
 */

/*
 * Copyright (c) <2008> Stefan Kalkowski
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef _SDL_config_genode_h
#define _SDL_config_genode_h

/* libC includes */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

/* Enable the Genode audio driver */
#define SDL_AUDIO_DRIVER_GENODE 1

/* Enable the stub cdrom driver (src/cdrom/dummy/\*.c) */
#define SDL_CDROM_DISABLED 1

/* Enable the stub joystick driver (src/joystick/dummy/\*.c) */
#define SDL_JOYSTICK_DISABLED 1

/* Enable the stub shared object loader (src/loadso/dummy/\*.c) */
#define SDL_LOADSO_DISABLED 1

/* Enable thread support */
#define SDL_THREAD_PTHREAD 1

/* Enable dummy timer support */
#define SDL_TIMER_DUMMY 1

/* Enable video drivers */
#define SDL_VIDEO_DRIVER_GENODE_FB 1

/* #define HAVE_MREMAP 0 */
#define HAVE_MALLOC
#define HAVE_CALLOC
#define HAVE_REALLOC
#define HAVE_FREE
#define HAVE_QSORT
#define HAVE_MEMSET
#define HAVE_MEMCMP
#define HAVE_STRLEN
#define HAVE_STRLCPY
#define HAVE_STRLCAT
#define HAVE_STRNCMP
#define HAVE_STRNCASECMP
#define HAVE_SNPRINTF
#define HAVE_VSNPRINTF

/* #define SDL_malloc	malloc */
/* #define SDL_calloc	calloc */
/* #define SDL_realloc	realloc */
/* #define SDL_free	free */

#endif /* _SDL_config_genode_h */
