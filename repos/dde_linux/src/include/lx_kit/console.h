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

#ifndef _LX_KIT__CONSOLE_H_
#define _LX_KIT__CONSOLE_H_

namespace Lx_kit {
	class Console;

	using namespace Genode;
}


class Lx_kit::Console
{
	private:

		enum { BUF_SIZE = 216 };

		char     _buf[BUF_SIZE + 1];
		unsigned _idx  = 0;

		void _out_char(char c);
		void _out_string(const char *str);
		void _flush();

	public:

		void print_string(const char *str);
};

#endif /* _LX_KIT__CONSOLE_H_ */
