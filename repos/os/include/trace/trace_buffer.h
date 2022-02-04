/*
 * \brief  Wrapper for Trace::Buffer that adds some convenient functionality
 * \author Martin Stein
 * \date   2018-01-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE__TRACE_BUFFER_H_
#define _TRACE__TRACE_BUFFER_H_

/* Genode includes */
#include <base/trace/buffer.h>


/**
 * Wrapper for Trace::Buffer that adds some convenient functionality
 */
class Trace_buffer
{
	private:

		Genode::Trace::Buffer        &_buffer;
		Genode::Trace::Buffer::Entry  _curr          { _buffer.first() };
		unsigned                      _wrapped_count { 0 };

	public:

		Trace_buffer(Genode::Trace::Buffer &buffer) : _buffer(buffer) { }

		/**
		 * Call functor for each entry that wasn't yet processed
		 */
		template <typename FUNC>
		void for_each_new_entry(FUNC && functor, bool update = true)
		{
			using namespace Genode;

			bool wrapped = _buffer.wrapped() != _wrapped_count;
			if (wrapped) {
				if ((_buffer.wrapped() - 1) != _wrapped_count) {
					warning("buffer wrapped multiple times; you might want to raise buffer size; curr_count=",
					        _buffer.wrapped(), " last_count=", _wrapped_count);
					_curr = _buffer.first();
				}
				_wrapped_count = (unsigned)_buffer.wrapped();
			}

			Trace::Buffer::Entry entry    { _curr };

			/**
			 * Iterate over all entries that were not processed yet.
			 *
			 * A note on terminology: The head of the buffer marks the write
			 * position. The first entry is the one that starts at the lowest
			 * memory address. The next entry returns an invalid entry called
			 * if the 'last' end of the buffer (highest address) was reached.
			 */
			for (; !entry.head(); entry = _buffer.next(entry)) {
				/* continue at first entry if we hit the end of the buffer */
				if (entry.last())
					entry = _buffer.first();

				/* skip empty entries */
				if (entry.empty())
					continue;

				/* functor may return false to continue processing later on */
				if (!functor(entry))
					break;
			}

			/* remember the next to be processed entry in _curr */
			if (update) _curr = entry;
		}

		void * address()        const { return &_buffer; }
};


#endif /* _TRACE__TRACE_BUFFER_H_ */
