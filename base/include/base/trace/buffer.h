/*
 * \brief  Event tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		struct Entry
		{
			size_t len;
			char   data[0];
		};

		Entry _entries[0];

		Entry *_head_entry() { return (Entry *)((addr_t)_entries + _head_offset); }

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
		}

		char *reserve(size_t len)
		{
			if (_head_offset + sizeof(Entry) + len <= _size)
				return _head_entry()->data;

			/* mark last entry with len 0 and wrap */
			_head_entry()->len = 0;
			_head_offset = 0;

			return _head_entry()->data;
		}

		void commit(size_t len)
		{
			/* omit empty entries */
			if (len == 0)
				return;

			_head_entry()->len = len;

			/* advance head offset, wrap when reaching buffer boundary */
			_head_offset += sizeof(Entry) + len;
			if (_head_offset == _size)
				_head_offset = 0;
		}


		/********************************************
		 ** Functions called from the TRACE client **
		 ********************************************/

		addr_t entries()     const { return (addr_t)_entries; }
		addr_t head_offset() const { return (addr_t)_head_offset; }
};

#endif /* _INCLUDE__BASE__TRACE__BUFFER_H_ */
