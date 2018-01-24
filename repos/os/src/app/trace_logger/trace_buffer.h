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
		Genode::Trace::Buffer::Entry  _curr { _buffer.first() };

	public:

		Trace_buffer(Genode::Trace::Buffer &buffer) : _buffer(buffer) { }

		/**
		 * Call functor for each entry that wasn't yet processed
		 */
		template <typename FUNC>
		void for_each_new_entry(FUNC && functor)
		{
			using namespace Genode;

			/* initialize _curr if _buffer was empty until now */
			if (_curr.last())
				_curr = _buffer.first();

			/* iterate over all entries that were not processed yet */
			     Trace::Buffer::Entry e1 = _curr;
			for (Trace::Buffer::Entry e2 = _curr; !e2.last();
			                          e2 = _buffer.next(e2))
			{
				e1 = e2;
				functor(e1);
			}
			/* remember the last processed entry in _curr */
			_curr = e1;
		}
};


#endif /* _TRACE_BUFFER_H_ */
