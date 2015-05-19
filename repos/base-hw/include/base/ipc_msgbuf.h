/*
 * \brief  IPC message buffers
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-01-03
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

#include <base/native_capability.h>
#include <util/string.h>

namespace Genode
{
	class Native_utcb;

	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base;

	/**
	 * Instance of IPC message buffer with specified buffer size
	 *
	 * 'Msgbuf_base' must be the last class this class inherits from.
	 */
	template <unsigned BUF_SIZE> class Msgbuf;
}


class Genode::Msgbuf_base
{
	public:

		enum { MAX_CAP_ARGS = 4 };

	private:

		friend class Native_utcb;

		size_t            _size;               /* buffer size in bytes */
		Native_capability _caps[MAX_CAP_ARGS]; /* capability buffer    */
		size_t            _snd_cap_cnt = 0;    /* capability counter   */
		size_t            _rcv_cap_cnt = 0;    /* capability counter   */

	public:

		/*************************************************
		 ** 'buf' must be the last member of this class **
		 *************************************************/

		char buf[]; /* begin of actual message buffer */

		Msgbuf_base(size_t size) : _size(size) { }

		void const * base() const { return &buf; }

		/**
		 * Return size of message buffer
		 */
		size_t size() const { return _size; }

		/**
		 * Return address of message buffer
		 */
		void *addr() { return &buf[0]; }

		/**
		 * Reset capability buffer.
		 */
		void reset()
		{
			_snd_cap_cnt = 0;
			_rcv_cap_cnt = 0;
		}

		/**
		 * Return how many capabilities are accepted by this message buffer
		 */
		size_t cap_rcv_window() { return _rcv_cap_cnt; }

		/**
		 * Set how many capabilities are accepted by this message buffer
		 */
		void cap_rcv_window(size_t cnt) { _rcv_cap_cnt = cnt;  }

		/**
		 * Add capability to buffer
		 */
		void cap_add(Native_capability const &cap)
		{
			if (_snd_cap_cnt < MAX_CAP_ARGS)
				_caps[_snd_cap_cnt++] = cap;
		}

		/**
		 * Return last capability from buffer.
		 */
		Native_capability cap_get()
		{
			return (_rcv_cap_cnt < _snd_cap_cnt)
			       ? _caps[_rcv_cap_cnt++] : Native_capability();
		}
};


template <unsigned BUF_SIZE>
class Genode::Msgbuf : public Genode::Msgbuf_base
{
	public:

		/**************************************************
		 ** 'buf' must be the first member of this class **
		 **************************************************/

		char buf[BUF_SIZE];

		/**
		 * Constructor
		 */
		Msgbuf() : Msgbuf_base(BUF_SIZE) { }
};

#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
