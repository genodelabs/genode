/*
 * \brief  IPC message buffer layout
 * \author Norman Feske
 * \date   2015-05-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

#include <base/native_types.h>

namespace Genode {

	class Msgbuf_base;
	template <unsigned> struct Msgbuf;
}


class Genode::Msgbuf_base
{
	public:

		enum { MAX_CAPS_PER_MSG = 3 };

	protected:

		/*
		 * Resolve ambiguity if the header is included from a libc-using
		 * program.
		 */
		typedef Genode::size_t size_t;

		/*
		 * Capabilities to be transferred
		 */
		Native_capability _caps[MAX_CAPS_PER_MSG];
		size_t            _used_caps = 0;
		size_t            _read_cap_index = 0;

		/**
		 * Maximum size of plain-data message payload
		 */
		size_t const _size;

		/**
		 * Actual size of plain-data message payload
		 */
		size_t _used_size = 0;

		char _msg_start[];  /* symbol marks start of message buffer data */

		/*
		 * No member variables are allowed beyond this point!
		 */

		Msgbuf_base(size_t size) : _size(size) { }

	public:

		char buf[];

		/**
		 * Return size of message buffer
		 */
		size_t size() const { return _size; };

		void reset_caps()
		{
			for (Genode::size_t i = 0; i < _used_caps; i++)
				_caps[i] = Native_capability();

			_used_caps = 0;
		}

		void reset_read_cap_index()
		{
			_read_cap_index = 0;
		}

		/**
		 * Return pointer to start of message-buffer content
		 */
		void const *data() const { return &_msg_start[0]; };
		void       *data()       { return &_msg_start[0]; };

		/**
		 * Exception type
		 */
		class Too_many_caps : public Exception { };

		/**
		 * Called from '_marshal_capability'
		 */
		void append_cap(Native_capability const &cap)
		{
			if (_used_caps == MAX_CAPS_PER_MSG)
				throw Too_many_caps();

			_caps[_used_caps++] = cap;
		}

		/**
		 * Called from '_unmarshal_capability'
		 */
		Native_capability extract_cap()
		{
			if (_read_cap_index == _used_caps)
				return Native_capability();

			return _caps[_read_cap_index++];
		}

		/**
		 * Return number of marshalled capabilities
		 */
		size_t used_caps() const { return _used_caps; }

		Native_capability &cap(unsigned index)
		{
			return _caps[index];
		}
};


template <unsigned BUF_SIZE>
struct Genode::Msgbuf : Msgbuf_base
{
	/**
	 * Pump up IPC message buffer to specified buffer size
	 *
	 * XXX remove padding of 16
	 */
	char buf[BUF_SIZE + 16];

	Msgbuf() : Msgbuf_base(BUF_SIZE) { }
};


#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
