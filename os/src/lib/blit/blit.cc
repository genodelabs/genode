/*
 * \brief  Generic blitting function
 * \author Norman Feske
 * \date   2007-10-10
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <blit/blit.h>
#include <util/string.h>


extern "C" void blit(void *s, unsigned src_w,
                     void *d, unsigned dst_w,
                     int w, int h)
{
	char *src = (char *)s, *dst = (char *)d;

	for (int i = h; i--; src += src_w, dst += dst_w)
		Genode::memcpy(dst, src, w);
}
