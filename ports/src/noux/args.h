/*
 * \brief  Handling command-line for Noux processes
 * \author Norman Feske
 * \date   2011-01-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__ARGS_H_
#define _NOUX__ARGS_H_

/* Genode includes */
#include <os/attached_ram_dataspace.h>
#include <util/string.h>
#include <base/printf.h>

namespace Noux {

	class Args
	{
		protected:

			char * const _buf;
			size_t const _buf_size;
			size_t       _len;

			size_t _free_size() const
			{
				/*
				 * Keep space for two trailing zeros, indicating the end of the
				 * stream of strings.
				 */
				return _buf_size - _len - 2;
			}

		public:

			class Overrun { };

			/**
			 * Constructor
			 *
			 * \param buf       argument buffer
			 * \param buf_size  size of argument buffer in character,
			 *                  must be at least 2
			 */
			Args(char *buf, size_t buf_size)
			: _buf(buf), _buf_size(buf_size), _len(0)
			{
				if (buf_size <= 2)
					throw Overrun();

				/* search end of string stream (two subsequent zeros) */
				for (; (_buf[_len] || _buf[_len + 1]) && (_len < _buf_size - 2); _len++);

				/* ensure termination of argument buffer */
				_buf[_buf_size - 1] = 0;
				_buf[_buf_size - 2] = 0;
			}

			Args() : _buf(0), _buf_size(0), _len(0) { }

			bool valid() const { return _buf_size > 0; }

			size_t len() const { return _len; }

			char const * const base() const { return _buf; }

			void append(char const *arg)
			{
				size_t const arg_len = strlen(arg);

				if (arg_len > _free_size())
					throw Overrun();

				strncpy(_buf + _len, arg, _buf_size - _len);

				_len += arg_len + 1; /* keep null termination between strings */

				/* mark end of stream of strings */
				_buf[_len] = 0;
			}

			void dump()
			{
				for (unsigned i = 0, j = 0; _buf[i] && (i < _buf_size - 2);
				     i += strlen(&_buf[i]) + 1, j++)
					PINF("arg(%u): \"%s\"", j, &_buf[i]);
			}
	};

	class Args_dataspace : private Attached_ram_dataspace, public Args
	{
		public:

			Args_dataspace(size_t size, Args const &from = Args())
			:
				Attached_ram_dataspace(env()->ram_session(), size),
				Args(local_addr<char>(), size)
			{
				if (from.len() > size - 2)
					throw Overrun();

				memcpy(_buf, from.base(), from.len() + 1);
			}

			using Attached_ram_dataspace::cap;
	};
}

#endif /* _NOUX__ARGS_H_ */
