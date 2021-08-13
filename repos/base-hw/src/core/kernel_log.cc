/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-hw Core includes */
#include <kernel/main.h>
#include <kernel/log.h>


void Kernel::log(char const c)
{
	enum {
		ASCII_LINE_FEED = 10,
		ASCII_CARRIAGE_RETURN = 13,
	};

	if (c == ASCII_LINE_FEED)
		main_print_char(ASCII_CARRIAGE_RETURN);

	main_print_char(c);
}
