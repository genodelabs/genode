/*
 * \brief  Libxkbcommon-based keyboard-layout generator
 * \author Christian Helmuth
 * \date   2019-08-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/* Linux includes */
#include <cstdio>
#include <cstdlib>


struct Formatted
{
	char *_string;

	Formatted(char const *format, va_list list)
	{
		::vasprintf(&_string, format, list);
	}

	Formatted(char const *format, ...)
	{
		va_list list;
		va_start(list, format);
		::vasprintf(&_string, format, list);
		va_end(list);
	}

	~Formatted()
	{
		::free(_string);
	}

	char const * string() const { return _string; }
};


#endif /* _UTIL_H_ */
