/*
 * \brief  Linux-specific layout of IPC message buffer
 * \author Norman Feske
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

#include <base/stdint.h>

namespace Genode {

	class Ipc_marshaller;

	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base
	{
		public:

			enum { MAX_CAPS_PER_MSG = 8 };

		protected:

			friend class Ipc_marshaller;

			/*
			 * Capabilities (file descriptors) to be transferred
			 */
			int            _caps[MAX_CAPS_PER_MSG];
			Genode::size_t _used_caps = 0;
			Genode::size_t _read_cap_index = 0;

			/**
			 * Maximum size of plain-data message payload
			 */
			Genode::size_t const _capacity;

			/**
			 * Actual size of plain-data message payload
			 */
			Genode::size_t _data_size = 0;

			Msgbuf_base(size_t capacity) : _capacity(capacity) { }

			struct Headroom { long space[4]; } _headroom;

			char _msg_start[];  /* symbol marks start of message buffer data */

			/*
			 * No member variables are allowed beyond this point!
			 */

		public:

			template <typename T>
			T &header()
			{
				static_assert(sizeof(T) <= sizeof(Headroom),
				              "Header size exceeds message headroom");
				return *reinterpret_cast<T *>(_msg_start - sizeof(T));
			}

			/**
			 * Return size of message buffer
			 */
			Genode::size_t capacity() const { return _capacity; };

			/**
			 * Return pointer of message data payload
			 */
			void       *data()       { return &_msg_start[0]; };
			void const *data() const { return &_msg_start[0]; };

			size_t data_size() const { return _data_size; }

			void reset_caps() { _used_caps = 0; _read_cap_index = 0; }

			bool append_cap(int cap)
			{
				if (_used_caps == MAX_CAPS_PER_MSG)
					return false;

				_caps[_used_caps++] = cap;
				return true;
			}

			int read_cap()
			{
				if (_read_cap_index == _used_caps)
					return -1;

				return _caps[_read_cap_index++];
			}

			size_t used_caps() const { return _used_caps; }

			int cap(unsigned index) const
			{
				return index < _used_caps ? _caps[index] : -1;
			}
	};


	/**
	 * Pump up IPC message buffer to specified buffer size
	 */
	template <unsigned BUF_SIZE>
	class Msgbuf : public Msgbuf_base
	{
		public:

			char buf[BUF_SIZE];

			Msgbuf() : Msgbuf_base(BUF_SIZE) { }
	};
}


#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
