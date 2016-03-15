/*
 * \brief  Fiasco-specific layout of IPC message buffer
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
		private:

			size_t const _capacity;

		protected:

			size_t _data_size = 0;

			friend class Ipc_marshaller;

			Msgbuf_base(size_t capacity) : _capacity(capacity) { }

			struct Headroom { long space[4]; } _headroom;

			char buf[];

		public:

			template <typename T>
			T &header()
			{
				static_assert(sizeof(T) <= sizeof(Headroom),
				              "Header size exceeds message headroom");
				return *reinterpret_cast<T *>(buf - sizeof(T));
			}

			unsigned long &word(unsigned i)
			{
				return reinterpret_cast<unsigned long *>(buf)[i];
			}

			/**
			 * Return size of message buffer
			 */
			size_t capacity() const { return _capacity; };

			/**
			 * Return pointer of message data payload
			 */
			void       *data()       { return buf; };
			void const *data() const { return buf; };

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
