/*
 * \brief  Generic IPC infrastructure
 * \author Norman Feske
 * \date   2006-06-12
 *
 * Most of the marshalling and unmarshallung code is generic for IPC
 * implementations among different platforms. In addition to the generic
 * marshalling items, platform-specific marshalling items can be realized
 * via specialized stream operators defined in the platform-specific
 * 'base/ipc.h'.  Hence, this header file is never to be included directly.
 * It should only be included by a platform-specific 'base/ipc.h' file.
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

	class Native_connection_state;

	class Ipc_error;
	class Ipc_marshaller;
	class Ipc_unmarshaller;
	class Ipc_client;
	class Ipc_server;
}


/**
 * Exception type
 */
class Genode::Ipc_error : public Exception { };


/**
 * Marshal arguments into send message buffer
 */
class Genode::Ipc_marshaller
{
	protected:

		Msgbuf_base &_snd_msg;
		unsigned     _write_offset = 0;

	private:

		char        *_snd_buf      = (char *)_snd_msg.data();
		size_t const _snd_buf_size = _snd_msg.size();

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
class Genode::Ipc_unmarshaller
{
	protected:

		Msgbuf_base  &_rcv_msg;
		unsigned      _read_offset  = 0;

	private:

		char         *_rcv_buf      = (char *)_rcv_msg.data();
		size_t const  _rcv_buf_size = _rcv_msg.size();

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


class Genode::Ipc_client : public Ipc_marshaller, public Ipc_unmarshaller,
                           public Noncopyable
{
	protected:

		int _result = 0;   /* result of most recent call */

		Native_capability _dst;

		/**
		 * Set return value if call to server failed
		 */
		void ret(int retval)
		{
			reinterpret_cast<umword_t *>(_rcv_msg.data())[1] = retval;
		}

		void _call();

	public:

		enum { ERR_INVALID_OBJECT = -70000, };

		/**
		 * Constructor
		 */
		Ipc_client(Native_capability const &dst,
		           Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg,
		           unsigned short rcv_caps = ~0);

		/**
		 * Send RPC message and wait for result
		 *
		 * \throw Ipc_error
		 */
		void call()
		{
			_call();
			extract(_result);
			if (_result == ERR_INVALID_OBJECT)
				throw Ipc_error();
		}

		int result() const { return _result; }
};


class Genode::Ipc_server : public Ipc_marshaller, public Ipc_unmarshaller,
                           public Noncopyable, public Native_capability
{
	protected:

		bool _reply_needed;   /* false for the first reply_wait */

		Native_capability _caller;

		Native_connection_state &_rcv_cs;

		void _prepare_next_reply_wait();

		unsigned long _badge;

	public:

		/**
		 * Constructor
		 */
		Ipc_server(Native_connection_state &,
		           Msgbuf_base &snd_msg, Msgbuf_base &rcv_msg);

		~Ipc_server();

		/**
		 * Wait for incoming call
		 */
		void wait();

		/**
		 * Send reply to destination
		 */
		void reply();

		/**
		 * Send result of previous RPC request and wait for new one
		 */
		void reply_wait();

		/**
		 * Set return value of server call
		 */
		void ret(int retval)
		{
			reinterpret_cast<umword_t *>(_snd_msg.data())[1] = retval;
		}

		/**
		 * Read badge that was supplied with the message
		 */
		unsigned long badge() const { return _badge; }

		/**
		 * Set reply destination
		 */
		void caller(Native_capability const &caller)
		{
			_caller       = caller;
			_reply_needed = caller.valid();
		}

		Native_capability caller() const { return _caller; }
};

#endif /* _INCLUDE__BASE__IPC_H_ */
