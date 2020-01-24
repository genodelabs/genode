/*
 * \brief  Implementation of the output interface that buffers characters
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__BUFFERED_OUTPUT_H_
#define _INCLUDE__BASE__BUFFERED_OUTPUT_H_

#include <base/output.h>

namespace Genode { template <size_t, typename> class Buffered_output; }


/**
 * Implementation of the output interface that buffers characters
 *
 * \param BUF_SIZE  maximum number of characters to buffer before writing
 * \param WRITE_FN  functor called to writing the buffered characters to a
 *                  backend.
 *
 * The 'WRITE_FN' functor is called with a null-terminated 'char const *'
 * as argument.
 */
template <Genode::size_t BUF_SIZE, typename BACKEND_WRITE_FN>
class Genode::Buffered_output : public Output
{
	private:

		BACKEND_WRITE_FN _write_fn;
		char             _buf[BUF_SIZE];
		unsigned         _num_chars = 0;

		void _flush()
		{
			/* null-terminate string */
			_buf[_num_chars] = 0;
			_write_fn(_buf);

			/* restart with empty buffer */
			_num_chars = 0;
		}

	public:

		Buffered_output(BACKEND_WRITE_FN const &write_fn) : _write_fn(write_fn) { }

		~Buffered_output() { _flush(); }

		void out_char(char c) override
		{
			/* ensure enough buffer space for complete escape sequence */
			if ((c == 27) && (_num_chars + 8 > BUF_SIZE)) _flush();

			_buf[_num_chars++] = c;

			/* flush immediately on line break */
			if (c == '\n' || _num_chars >= sizeof(_buf) - 1)
				_flush();
		}
};

#endif /* _INCLUDE__BASE__BUFFERED_OUTPUT_H_ */
