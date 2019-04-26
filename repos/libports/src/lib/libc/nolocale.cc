/*
 * \brief  POSIX locale stubs
 * \author Emery Hemingway
 * \date   2019-04-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

extern "C" {

/* Libc includes */
#include <locale.h>
#include <runetype.h>
#include <xlocale_private.h>


extern struct xlocale_component __xlocale_global_collate;
extern struct xlocale_component __xlocale_global_ctype;
extern struct xlocale_component __xlocale_global_monetary;
extern struct xlocale_component __xlocale_global_numeric;
extern struct xlocale_component __xlocale_global_time;
extern struct xlocale_component __xlocale_global_messages;
extern struct xlocale_component __xlocale_C_collate;
extern struct xlocale_component __xlocale_C_ctype;

struct _xlocale __xlocale_global_locale = {
	{0},
	{
		&__xlocale_global_collate,
		&__xlocale_global_ctype,
		&__xlocale_global_monetary,
		&__xlocale_global_numeric,
		&__xlocale_global_time,
		&__xlocale_global_messages
	},
	0,
	0,
	1,
	0
};


locale_t
__get_locale(void)
{
	return &__xlocale_global_locale;
}


extern int _none_init(struct xlocale_ctype *l, _RuneLocale *rl);

char *setlocale(int, const char *)
{
	_none_init((xlocale_ctype*)&__xlocale_global_ctype,
	           (_RuneLocale*)&_DefaultRuneLocale);
	return (char*)"C";
}

}
