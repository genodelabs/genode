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

#include <base/ipc_msgbuf.h>
#include <base/rpc_args.h>
#include <base/log.h>

namespace Genode {

	struct Ipc_error : Exception { };

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
 * Unmarshal arguments from receive buffer
 */
class Genode::Ipc_unmarshaller : Noncopyable
{
	protected:

		Msgbuf_base &_rcv_msg;
		unsigned     _read_offset = 0;
		unsigned     _read_cap_index = 0;

	private:

		char         *_rcv_buf      = (char *)_rcv_msg.data();
		size_t const  _rcv_buf_size = _rcv_msg.capacity();

	public:

		/**
		 * Obtain typed capability from message buffer
		 */
		template <typename IT>
		void extract(Capability<IT> &typed_cap)
		{
			Native_capability untyped_cap;
			extract(untyped_cap);
			typed_cap = reinterpret_cap_cast<IT>(untyped_cap);
		}

		/**
		 * Obtain capability from message buffer
		 */
		void extract(Native_capability &cap)
		{
			cap = _read_cap_index < _rcv_msg.used_caps()
			    ? _rcv_msg.cap(_read_cap_index) : Native_capability();
			_read_cap_index++;
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
			if (_read_offset + size > _rcv_buf_size) {
				error("message buffer overrun");
				return;
			}

			b = Rpc_in_buffer<SIZE>(&_rcv_buf[_read_offset], size);
			_read_offset += align_natural(size);
		}

		/**
		 * Read value of type T from buffer
		 */
		template <typename T>
		void extract(T &value)
		{
			/* check receive buffer range */
			if (_read_offset + sizeof(T) > _rcv_buf_size) return;

			/* return value from receive buffer */
			value = *reinterpret_cast<T *>(&_rcv_buf[_read_offset]);

			/* increment read pointer to next dword-aligned value */
			_read_offset += align_natural(sizeof(T));
		}

		Ipc_unmarshaller(Msgbuf_base &rcv_msg) : _rcv_msg(rcv_msg) { }
};

#endif /* _INCLUDE__BASE__IPC_H_ */
