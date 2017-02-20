/*
 * \brief  Event tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TRACE__BUFFER_H_
#define _INCLUDE__BASE__TRACE__BUFFER_H_

#include <base/stdint.h>
#include <base/thread.h>
#include <cpu_session/cpu_session.h>

namespace Genode { namespace Trace { class Buffer; } }


/**
 * Buffer shared between CPU client thread and TRACE client
 */
class Genode::Trace::Buffer
{
	private:

		unsigned volatile _head_offset;  /* in bytes, relative to 'entries' */
		unsigned volatile _size;         /* in bytes */
		unsigned volatile _wrapped;      /* count of buffer wraps */

		struct _Entry
		{
			size_t len;
			char   data[0];
		};

		_Entry _entries[0];

		_Entry *_head_entry() { return (_Entry *)((addr_t)_entries + _head_offset); }

		void _buffer_wrapped()
		{
			_head_offset = 0;
			_wrapped++;
		}

		/*
		 * The 'entries' member marks the beginning of the trace buffer
		 * entries. No other member variables must follow.
		 */

	public:

		/******************************************
		 ** Functions called from the CPU client **
		 ******************************************/

		void init(size_t size)
		{
			_head_offset = 0;

			/* compute number of bytes available for tracing data */
			size_t const header_size = (addr_t)&_entries - (addr_t)this;

			_size = size - header_size;

			_wrapped = 0;
		}

		char *reserve(size_t len)
		{
			if (_head_offset + sizeof(_Entry) + len <= _size)
				return _head_entry()->data;

			/* mark last entry with len 0 and wrap */
			if (_head_offset + sizeof(_Entry) <= _size)
				_head_entry()->len = 0;

			_buffer_wrapped();

			return _head_entry()->data;
		}

		void commit(size_t len)
		{
			/* omit empty entries */
			if (len == 0)
				return;

			_head_entry()->len = len;

			/* advance head offset, wrap when reaching buffer boundary */
			_head_offset += sizeof(_Entry) + len;
			if (_head_offset == _size)
				_buffer_wrapped();
		}

		unsigned wrapped() const { return _wrapped; }


		/********************************************
		 ** Functions called from the TRACE client **
		 ********************************************/

		class Entry
		{
			private:

				_Entry const *_entry;

				friend class Buffer;

				Entry(_Entry const *entry) : _entry(entry) { }

			public:

				size_t      length() const { return _entry->len; }
				char const *data()   const { return _entry->data; }
				bool        last()   const { return _entry == 0; }

				/*
				 * \deprecated use 'last' instead
				 */
				bool is_last() const { return last(); }
		};

		Entry first() const
		{
			return _entries->len ? Entry(_entries) : Entry(0);
		}

		Entry next(Entry entry) const
		{
			if (entry.last())
				return Entry(0);

			if (entry.length() == 0)
				return Entry(0);

			addr_t const offset = (addr_t)entry._entry - (addr_t)_entries;
			if (offset + entry.length() + sizeof(_Entry) > _size)
				return Entry(0);

			return Entry((_Entry const *)((addr_t)entry.data() + entry.length()));
		}
};

#endif /* _INCLUDE__BASE__TRACE__BUFFER_H_ */
