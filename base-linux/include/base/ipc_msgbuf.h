/*
 * \brief  Linux-specific layout of IPC message buffer
 * \author Norman Feske
 * \date   2006-06-14
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
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
		protected:

			Genode::size_t _size;
			char           _msg_start[];  /* symbol marks start of message buffer data */

			/*
			 * No member variables are allowed beyond this point!
			 */

		public:

			char buf[];

			/**
			 * Return size of message buffer
			 */
			inline Genode::size_t size() const { return _size; };

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &_msg_start[0]; };
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
