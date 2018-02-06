/*
 * \brief  Buffer for storing decoded characters
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TERMINAL__READ_BUFFER_H_
#define _TERMINAL__READ_BUFFER_H_

#include <os/ring_buffer.h>
#include <base/signal.h>


namespace Terminal {

	class Read_buffer;

	enum { READ_BUFFER_SIZE = 4096 };
}


class Terminal::Read_buffer : public Genode::Ring_buffer<unsigned char, READ_BUFFER_SIZE>
{
	private:

		Genode::Signal_context_capability _sigh_cap { };

	public:

		/**
		 * Register signal handler for read-avail signals
		 */
		void sigh(Genode::Signal_context_capability cap) { _sigh_cap = cap; }

		/**
		 * Add element into read buffer and emit signal
		 */
		void add(unsigned char c)
		{
			Genode::Ring_buffer<unsigned char, READ_BUFFER_SIZE>::add(c);

			if (_sigh_cap.valid())
				Genode::Signal_transmitter(_sigh_cap).submit();
		}

		void add(char const *str)
		{
			while (*str)
				add(*str++);
		}
};

#endif /* _TERMINAL__READ_BUFFER_H_ */
