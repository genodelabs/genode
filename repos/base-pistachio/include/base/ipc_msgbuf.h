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

	class Ipc_marshaller;

	/**
	 * IPC message buffer layout
	 */
	class Msgbuf_base
	{
		protected:

			friend class Ipc_marshaller;

			size_t const _capacity;

			size_t _data_size = 0;

			char _msg_start[];  /* symbol marks start of message */

			Msgbuf_base(size_t capacity) : _capacity(capacity) { }

		public:

			/*
			 * Begin of message buffer layout
			 */
			Pistachio::L4_Fpage_t rcv_fpage;

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
