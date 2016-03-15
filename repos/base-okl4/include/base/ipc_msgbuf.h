/*
 * \brief  OKL4-specific layout of IPC message buffer
 * \author Norman Feske
 * \date   2009-03-25
 *
 * On OKL4, we do not directly use the a kernel-specific message-buffer layout.
 * The IPC goes through the UTCBs of the sending and receiving threads. Because
 * on Genode, message buffers are decoupled from threads, we need to copy-in
 * and copy-out the message payload between the message buffers and the used
 * UTCBs.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

#include <base/stdint.h>
#include <base/printf.h>

namespace Genode {

	class Ipc_marshaller;

	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base
	{
		protected:

			size_t _data_size = 0;
			size_t _capacity;
			char   _msg_start[];  /* symbol marks start of message */

			Msgbuf_base(size_t capacity) : _capacity(capacity) { }

			friend class Ipc_marshaller;

		public:

			/**
			 * Return size of message buffer
			 */
			size_t capacity() const { return _capacity; };

			/**
			 * Return pointer of message data payload
			 */
			void       *data()       { return &_msg_start[0]; };
			void const *data() const { return &_msg_start[0]; };

			size_t data_size() const { return _data_size; }
	};


	/**
	 * Instance of IPC message buffer with specified buffer size
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
