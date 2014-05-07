/*
 * \brief  Pistachio-specific layout of IPC message buffer
 * \author Julian Stecklina
 * \date   2007-01-10
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
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

			size_t _size;
			char   _msg_start[];  /* symbol marks start of message */

		public:

			/*
			 * Begin of message buffer layout
			 */
			Pistachio::L4_Fpage_t rcv_fpage;
			/* Send message */
			/* Recv message */
			char buf[];

			/**
			 * Return size of message buffer
			 */
			inline size_t size() const { return _size; };

			/**
			 * Return address of message buffer
			 */
			inline void *addr() { return &_msg_start[0]; };
	};


	/**
	 * Instance of IPC message buffer with specified buffer size
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
