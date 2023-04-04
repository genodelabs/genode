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
		using Entry = Genode::Trace::Buffer::Entry;

		Genode::Trace::Buffer        &_buffer;
		Entry                         _curr { Entry::invalid() };
		unsigned long long            _lost_count { 0 };

	public:

		Trace_buffer(Genode::Trace::Buffer &buffer) : _buffer(buffer) { }

		/**
		 * Call functor for each entry that wasn't yet processed
		 */
		template <typename FUNC>
		void for_each_new_entry(FUNC && functor, bool update = true)
		{
			using namespace Genode;

			if (!_buffer.initialized())
				return;

			bool lost = _buffer.lost_entries() != _lost_count;
			if (lost) {
				warning("lost ", _buffer.lost_entries() - _lost_count,
				        ", entries; you might want to raise buffer size");
				_lost_count = (unsigned)_buffer.lost_entries();
			}

			Entry entry { _curr };

			/**
			 * Iterate over all entries that were not processed yet.
			 *
			 * A note on terminology: The head of the buffer marks the write
			 * position. The first entry is the one that starts at the lowest
			 * memory address. The next entry returns an invalid entry
			 * if the 'last' entry of the buffer (highest address) was reached.
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

		void * address() const { return &_buffer; }

		bool empty() const { return !_buffer.initialized() || _curr.head(); }
};

#endif /* _TRACE__TRACE_BUFFER_H_ */
