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
		void for_each_new_entry(FUNC && functor, bool update = true)
		{
			using namespace Genode;

			bool wrapped = _buffer.wrapped() != _wrapped_count;
			if (wrapped)
				_wrapped_count = _buffer.wrapped();

			Trace::Buffer::Entry new_curr { _curr };
			Trace::Buffer::Entry entry    { _curr };

			/**
			 * If '_curr' is marked 'last' (i.e., the entry pointer it contains
			 * is invalid), either this method wasn't called before on this
			 * buffer or the buffer was empty on all previous calls (note that
			 * '_curr' is only updated with valid entry pointers by this
			 * method). In this case, we start with the first entry (if any).
			 *
			 * If '_curr' is not marked 'last' it points to the last processed
			 * entry and we proceed with the, so far unprocessed entry next to
			 * it (if any). Note that in this case, the entry behind '_curr'
			 * might have got overridden since the last call to this method
			 * because the buffer wrapped and oustripped the entry consumer.
			 * This problem is well known and should be avoided by choosing a
			 * large enough buffer.
			 */
			if (entry.last())
				entry = _buffer.first();
			else
				entry = _buffer.next(entry);

			/* iterate over all entries that were not processed yet */
			for (; wrapped || !entry.last(); entry = _buffer.next(entry)) {
				/* if buffer wrapped, we pass the last entry once and continue at first entry */
				if (wrapped && entry.last()) {
					wrapped = false;
					entry = _buffer.first();
					if (entry.last())
						break;
				}

				if (!functor(entry))
					break;

				new_curr = entry;
			}

			/* remember the last processed entry in _curr */
			if (update) _curr = new_curr;
		}

		void * address()        const { return &_buffer; }
};


#endif /* _TRACE_BUFFER_H_ */
