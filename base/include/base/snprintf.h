/*
 * \brief  Facility to write format string into character buffer
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SNPRINTF_H_
#define _INCLUDE__BASE__SNPRINTF_H_

#include <base/console.h>
#include <base/stdint.h>

namespace Genode {

	class String_console : public Console
	{
		private:

			char   *_dst;
			size_t  _dst_len;
			size_t  _w_offset;

		public:

			/**
			 * Constructor
			 *
			 * \param dst      destination character buffer
			 * \param dst_len  size of dst
			 */
			String_console(char *dst, size_t dst_len)
			: _dst(dst), _dst_len(dst_len), _w_offset(0)
			{ _dst[0] = 0; }

			/**
			 * Return number of characters in destination buffer
			 */
			size_t len() { return _w_offset; }


			/***********************
			 ** Console interface **
			 ***********************/

			void _out_char(char c)
			{
				/* ensure to leave space for null-termination */
				if (_w_offset + 2 > _dst_len)
					return;

				_dst[_w_offset++] = c;
				_dst[_w_offset] = 0;
			}
	};

	/**
	 * Print format string into character buffer
	 *
	 * \return  number of characters written to destination buffer
	 */
	inline int snprintf(char *, size_t, const char *, ...) __attribute__((format(printf, 3, 4)));
	inline int snprintf(char *dst, size_t dst_len, const char *format, ...)
	{
		va_list list;
		va_start(list, format);

		String_console sc(dst, dst_len);
		sc.vprintf(format, list);

		va_end(list);
		return sc.len();
	}
}

#endif /* _INCLUDE__BASE__SNPRINTF_H_ */
