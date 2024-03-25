/*
 * \brief  Lx_kit format string backend
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2021-03-17
 *
 * Greatly inspired by the former DDE Linux Lx_kit implementation.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* local includes */
#include <lx_kit/console.h>


void Lx_kit::Console::_flush()
{
	if (!_idx)
		return;

	_buf[_idx] = 0;
	log(Cstring(_buf));
	_idx = 0;
}


void Lx_kit::Console::_out_char(char c)
{
	if (c == '\n' || _idx == BUF_SIZE || c == 0)
		_flush();
	else
		_buf[_idx++] = c;
}


void Lx_kit::Console::_out_string(const char *str)
{
	if (str)
		while (*str) _out_char(*str++);
	else
		_flush();
}


void Lx_kit::Console::print_string(const char *str)
{
	if (!str) {
		_out_string("<null string>");
		return;
	}

	/* strip potential control characters for log level */
	if (*str == 1)
		str += 2;

	_out_string(str);
}
