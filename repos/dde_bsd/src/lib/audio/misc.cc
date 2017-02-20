/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-09
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/snprintf.h>
#include <util/string.h>

/* local includes */
#include <bsd_emul.h>

/* compiler includes */
#include <stdarg.h>


/*******************
 ** machine/mutex **
 *******************/

void mtx_enter(struct mutex *mtx) {
	mtx->mtx_owner = curcpu(); }


void mtx_leave(struct mutex *mtx) {
	mtx->mtx_owner = nullptr; }


/*****************
 ** sys/systm.h **
 *****************/

extern "C" void panic(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);

	Genode::sleep_forever();
}


extern "C" int printf(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);

	return 0; /* XXX proper return value */
}


extern "C" int snprintf(char *str, size_t size, const char *format, ...)
{
	va_list list;

	va_start(list, format);
	Genode::String_console sc(str, size);
	sc.vprintf(format, list);
	va_end(list);

	return sc.len();
}


extern "C" int strcmp(const char *s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

extern "C" size_t strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';              /* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);    /* count does not include NUL */
}
