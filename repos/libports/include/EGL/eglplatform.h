/*
 * \brief  Genode-specific EGL platform definitions
 * \author Norman Feske
 * \date   2010-07-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __eglplatform_h_  /* include-guard named as on the other platforms */
#define __eglplatform_h_

#include <KHR/khrplatform.h>

#ifndef EGLAPI
#define EGLAPI KHRONOS_APICALL
#endif

#ifndef EGLAPIENTRY
#define EGLAPIENTRY  KHRONOS_APIENTRY
#endif
#define EGLAPIENTRYP EGLAPIENTRY*

typedef int   EGLNativeDisplayType;
typedef void *EGLNativePixmapType;

struct Genode_egl_window
{
	int width;
	int height;
	unsigned char *addr;
};

typedef struct Genode_egl_window *EGLNativeWindowType;

typedef khronos_int32_t EGLint;

#endif /* __eglplatform_h_ */
