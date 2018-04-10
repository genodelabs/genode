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

#ifndef _TRACE_BUFFER_H_
#define _TRACE_BUFFER_H_

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
		void for_each_new_entry(FUNC && functor)
		{
			using namespace Genode;

			bool wrapped = _buffer.wrapped() != _wrapped_count;
			if (wrapped)
				_wrapped_count = _buffer.wrapped();

			/* initialize _curr if _buffer was empty until now */
			if (_curr.last())
				_curr = _buffer.first();

			/* iterate over all entries that were not processed yet */
			     Trace::Buffer::Entry e1 = _curr;
			for (Trace::Buffer::Entry e2 = _curr; wrapped || !e2.last();
			                          e2 = _buffer.next(e2))
			{
				/* if buffer wrapped, we pass the last entry once and continue at first entry */
				if (wrapped && e2.last()) {
					wrapped = false;
					e2 = _buffer.first();
					if (e2.last())
						break;
				}

				e1 = e2;
				functor(e1);
			}
			/* remember the last processed entry in _curr */
			_curr = e1;
		}
};


#endif /* _TRACE_BUFFER_H_ */
