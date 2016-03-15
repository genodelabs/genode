/*
 * \brief  Generic IPC infrastructure
 * \author Norman Feske
 * \date   2006-06-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_H_
#define _INCLUDE__BASE__IPC_H_

#include <util/misc_math.h>
#include <util/string.h>
#include <util/noncopyable.h>
#include <base/exception.h>
#include <base/capability.h>
#include <base/ipc_msgbuf.h>
#include <base/rpc_args.h>
#include <base/printf.h>

namespace Genode {

	class Ipc_error;
	class Ipc_marshaller;
	class Ipc_unmarshaller;

	/**
	 * Invoke capability to call an RPC function
	 *
	 * \param rcv_caps  number of capabilities expected as result
	 * \throw Blocking_canceled
	 *
	 * \noapi
	 *
	 * The 'rcv_caps' value is used on kernels like NOVA to allocate the
	 * receive window for incoming capability selectors.
	 */
	Rpc_exception_code ipc_call(Native_capability dst,
	                            Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
	                            size_t rcv_caps);
}


/**
 * Exception type
 */
class Genode::Ipc_error : public Exception { };


/**
 * Marshal arguments into send message buffer
 */
class Genode::Ipc_marshaller : Noncopyable
{
	protected:

		Msgbuf_base &_snd_msg;
		size_t      &_write_offset = _snd_msg._data_size;

	private:

		char        *_snd_buf      = (char *)_snd_msg.data();
		size_t const _snd_buf_size = _snd_msg.capacity();

	public:

		/**
		 * Write value to send buffer
		 */
		template <typename T>
		void insert(T const &value)
		{
			/* check buffer range */
			if (_write_offset + sizeof(T) >= _snd_buf_size) return;

			/* write integer to buffer */
			*reinterpret_cast<T *>(&_snd_buf[_write_offset]) = value;

			/* increment write pointer to next dword-aligned value */
			_write_offset += align_natural(sizeof(T));
		}

		template <size_t MAX_BUFFER_SIZE>
		void insert(Rpc_in_buffer<MAX_BUFFER_SIZE> const &b)
		{
			size_t const size = b.size();
			insert(size);
			insert(b.base(), size);
		}

		/**
		 * Write bytes to send buffer
		 */
		void insert(char const *src_addr, unsigned num_bytes)
		{
			/* check buffer range */
			if (_write_offset + num_bytes >= _snd_buf_size) return;

			/* copy buffer */
			memcpy(&_snd_buf[_write_offset], src_addr, num_bytes);

			/* increment write pointer to next dword-aligned value */
			_write_offset += align_natural(num_bytes);
		}

		/**
		 * Insert capability to message buffer
		 */
		void insert(Native_capability const &cap);

		template <typename IT>
		void insert(Capability<IT> const &typed_cap)
		{
			Native_capability untyped_cap = typed_cap;
			insert(untyped_cap);
		}

		Ipc_marshaller(Msgbuf_base &snd_msg) : _snd_msg(snd_msg) { }
};


/**
 * Unmarshal arguments from receive buffer
 */
class Genode::Ipc_unmarshaller : Noncopyable
{
	protected:

		Msgbuf_base  &_rcv_msg;
		unsigned      _read_offset = 0;

	private:

		char         *_rcv_buf      = (char *)_rcv_msg.data();
		size_t const  _rcv_buf_size = _rcv_msg.capacity();

	public:

		template <typename IT>
		void extract(Capability<IT> &typed_cap)
		{
			Native_capability untyped_cap;
			extract(untyped_cap);
			typed_cap = reinterpret_cap_cast<IT>(untyped_cap);
		}

		/**
		 * Read 'Rpc_in_buffer' from receive buffer
		 */
		template <size_t SIZE>
		void extract(Rpc_in_buffer<SIZE> &b)
		{
			size_t size = 0;
			extract(size);
			b = Rpc_in_buffer<SIZE>(0, 0);

			/*
			 * Check receive buffer range
			 *
			 * Note: The addr of the Rpc_in_buffer_base is a null pointer when this
			 *       condition triggers.
			 */
			if (_read_offset + size >= _rcv_buf_size) {
				PERR("message buffer overrun");
				return;
			}

			b = Rpc_in_buffer<SIZE>(&_rcv_buf[_read_offset], size);
			_read_offset += align_natural(size);
		}

		/**
		 * Obtain capability from message buffer
		 */
		void extract(Native_capability &cap);

		/**
		 * Read value of type T from buffer
		 */
		template <typename T>
		void extract(T &value)
		{
			/* check receive buffer range */
			if (_read_offset + sizeof(T) >= _rcv_buf_size) return;

			/* return value from receive buffer */
			value = *reinterpret_cast<T *>(&_rcv_buf[_read_offset]);

			/* increment read pointer to next dword-aligned value */
			_read_offset += align_natural(sizeof(T));
		}

	public:

		Ipc_unmarshaller(Msgbuf_base &rcv_msg) : _rcv_msg(rcv_msg) { }
};

#endif /* _INCLUDE__BASE__IPC_H_ */
