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

namespace Genode {

	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base
	{
		public:

			enum { MAX_CAPS_PER_MSG = 8 };

		protected:

			/*
			 * Capabilities (file descriptors) to be transferred
			 */
			int            _caps[MAX_CAPS_PER_MSG];
			Genode::size_t _used_caps;
			Genode::size_t _read_cap_index;

			/**
			 * Maximum size of plain-data message payload
			 */
			Genode::size_t _size;

			/**
			 * Actual size of plain-data message payload
			 */
			Genode::size_t _used_size;

			char _msg_start[];  /* symbol marks start of message buffer data */

			/*
			 * No member variables are allowed beyond this point!
			 */

		public:

			char buf[];

			Msgbuf_base() { reset_caps(); }

			/**
			 * Return size of message buffer
			 */
			inline Genode::size_t size() const { return _size; };

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &_msg_start[0]; };

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

			Msgbuf() { _size = BUF_SIZE; }
	};
}


#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
