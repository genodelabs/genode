/*
 * \brief  IPC message buffers
 * \author Martin Stein
 * \date   2012-01-03
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

namespace Genode
{
	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base
	{
		protected:

			size_t _size; /* buffer size in bytes */

		public:

			char buf[]; /* begin of actual message buffer */

			/*************************************************
			 ** 'buf' must be the last member of this class **
			 *************************************************/

			/**
			 * Return size of message buffer
			 */
			inline size_t size() const { return _size; }

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &buf[0]; }
	};

	/**
	 * Instance of IPC message buffer with specified buffer size
	 *
	 * 'Msgbuf_base' must be the last class this class inherits from.
	 */
	template <unsigned BUF_SIZE>
	class Msgbuf : public Msgbuf_base
	{
		public:

			/**************************************************
			 ** 'buf' must be the first member of this class **
			 **************************************************/

			char buf[BUF_SIZE];

			/**
			 * Constructor
			 */
			Msgbuf() { _size = BUF_SIZE; }
	};
}

#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */

