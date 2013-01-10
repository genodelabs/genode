/*
 * \brief  Simple console for debug output
 * \author Norman Feske
 * \date   2006-04-07
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CONSOLE_H_
#define _INCLUDE__BASE__CONSOLE_H_

#include <stdarg.h>

namespace Genode {

	class Console
	{
		public:

			virtual ~Console() {}

			/**
			 * Print format string
			 */
			void  printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
			void vprintf(const char *format, va_list) __attribute__((format(printf, 2, 0)));

		protected:

			/**
			 * Backend function for the output of one character
			 */
			virtual void _out_char(char c) = 0;

			/**
			 * Backend function for the output of a null-terminated string
			 *
			 * The default implementation uses _out_char. This function may
			 * be overridden by the backend for improving efficiency.
			 *
			 * This function is virtual to enable the use an optimized
			 * string-output functions on some target platforms, e.g.
			 * a kernel debugger that offers a string-output syscall. The
			 * default implementation calls '_out_char' for each character.
			 */
			virtual void _out_string(const char *str);

		private:

			template <typename T> void _out_unsigned(T value, unsigned base = 10, int pad = 0);
			template <typename T> void _out_signed(T value, unsigned base = 10);
	};
}

#endif /* _INCLUDE__BASE__CONSOLE_H_ */
