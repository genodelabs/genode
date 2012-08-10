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
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__IPC_GENERIC_H_
#define _INCLUDE__BASE__IPC_GENERIC_H_

#include <util/misc_math.h>
#include <util/string.h>
#include <base/errno.h>
#include <base/exception.h>
#include <base/capability.h>
#include <base/ipc_msgbuf.h>
#include <base/rpc_args.h>
#include <base/printf.h>

namespace Genode {

	enum Ipc_ostream_send      { IPC_SEND };
	enum Ipc_istream_wait      { IPC_WAIT };
	enum Ipc_client_call       { IPC_CALL };
	enum Ipc_server_reply      { IPC_REPLY };
	enum Ipc_server_reply_wait { IPC_REPLY_WAIT };


	/*********************
	 ** Exception types **
	 *********************/

	class Ipc_error : public Exception { };


	/**
	 * Marshal arguments into send message buffer
	 */
	class Ipc_marshaller
	{
		protected:

			char        *_sndbuf;
			size_t       _sndbuf_size;
			unsigned     _write_offset;

		protected:

			/**
			 * Write value to send buffer
			 */
			template <typename T>
			void _write_to_buf(T const &value)
			{
				/* check buffer range */
				if (_write_offset + sizeof(T) >= _sndbuf_size) return;

				/* write integer to buffer */
				*reinterpret_cast<T *>(&_sndbuf[_write_offset]) = value;

				/* increment write pointer to next dword-aligned value */
				_write_offset += align_natural(sizeof(T));
			}

			/**
			 * Write bytes to send buffer
			 */
			void _write_to_buf(char const *src_addr, unsigned num_bytes)
			{
				/* check buffer range */
				if (_write_offset + num_bytes >= _sndbuf_size) return;

				/* copy buffer */
				memcpy(&_sndbuf[_write_offset], src_addr, num_bytes);

				/* increment write pointer to next dword-aligned value */
				_write_offset += align_natural(num_bytes);
			}

			/**
			 * Write 'Rpc_in_buffer' to send buffer
			 */
			void _write_buffer_to_buf(Rpc_in_buffer_base const &b)
			{
				size_t size = b.size();
				_write_to_buf(size);
				_write_to_buf(b.base(), size);
			}

			/**
			 * Write array to send buffer
			 */
			template <typename T, size_t N>
			void _write_to_buf(T const (&array)[N])
			{
				/* check buffer range */
				if (_write_offset + sizeof(array) >= _sndbuf_size)
					PERR("send buffer overrun");

				memcpy(&_sndbuf[_write_offset], array, sizeof(array));
				_write_offset += align_natural(sizeof(array));
			}

		public:

			Ipc_marshaller(char *sndbuf, size_t sndbuf_size)
			: _sndbuf(sndbuf), _sndbuf_size(sndbuf_size), _write_offset(0) { }
	};


	/**
	 * Unmarshal arguments from receive buffer
	 */
	class Ipc_unmarshaller
	{
		protected:

			char        *_rcvbuf;
			size_t       _rcvbuf_size;
			unsigned     _read_offset;

		protected:

			/**
			 * Read value of type T from buffer
			 */
			template <typename T>
			void _read_from_buf(T &value)
			{
				/* check receive buffer range */
				if (_read_offset + sizeof(T) >= _rcvbuf_size) return;

				/* return value from receive buffer */
				value = *reinterpret_cast<T *>(&_rcvbuf[_read_offset]);

				/* increment read pointer to next dword-aligned value */
				_read_offset += align_natural(sizeof(T));
			}

			/**
			 * Read 'Rpc_in_buffer' from receive buffer
			 */
			void _read_bytebuf_from_buf(Rpc_in_buffer_base &b)
			{
				size_t size = 0;
				_read_from_buf(size);
				b = Rpc_in_buffer_base(0, 0);

				/*
				 * Check receive buffer range
				 *
				 * Note: The addr of the Rpc_in_buffer_base is a null pointer when this
				 *       condition triggers.
				 */
				if (_read_offset + size >= _rcvbuf_size) {
					PERR("message buffer overrun");
					return;
				}

				b = Rpc_in_buffer_base(&_rcvbuf[_read_offset], size);
				_read_offset += align_natural(size);
			}

			/**
			 * Read array from receive buffer
			 */
			template <typename T, size_t N>
			void _read_from_buf(T (&array)[N])
			{
				if (_read_offset + sizeof(array) >= _rcvbuf_size) {
					PERR("receive buffer overrun");
					return;
				}

				memcpy(array, &_rcvbuf[_read_offset], sizeof(array));
				_read_offset += align_natural(sizeof(array));
			}

			/**
			 * Read long value at specified byte index of receive buffer
			 */
			long _long_at_idx(int idx) { return *(long *)(&_rcvbuf[idx]); }

		public:

			Ipc_unmarshaller(char *rcvbuf, size_t rcvbuf_size)
			: _rcvbuf(rcvbuf), _rcvbuf_size(rcvbuf_size), _read_offset(0) { }
	};


	/**
	 * Stream for sending information via a capability to an endpoint
	 */
	class Ipc_ostream : public Ipc_marshaller
	{
		protected:

			Msgbuf_base      *_snd_msg;  /* send message buffer */
			Native_capability _dst;

			/**
			 * Reset marshaller and write badge at the beginning of the message
			 */
			void _prepare_next_send();

			/**
			 * Send message in _snd_msg to _dst
			 */
			void _send();

			/**
			 * Insert capability to message buffer
			 */
			void _marshal_capability(Native_capability const &cap);

		public:

			/**
			 * Constructor
			 */
			Ipc_ostream(Native_capability dst, Msgbuf_base *snd_msg);

			/**
			 * Return true if Ipc_ostream is ready for send
			 */
			bool ready_for_send() const { return _dst.valid(); }

			/**
			 * Insert value into send buffer
			 */
			template <typename T>
			Ipc_ostream &operator << (T const &value)
			{
				_write_to_buf(value);
				return *this;
			}

			/**
			 * Insert byte buffer to send buffer
			 */
			Ipc_ostream &operator << (Rpc_in_buffer_base const &b)
			{
				_write_buffer_to_buf(b);
				return *this;
			}

			/**
			 * Insert capability to send buffer
			 */
			Ipc_ostream &operator << (Native_capability const &cap)
			{
				_marshal_capability(cap);
				return *this;
			}

			/**
			 * Insert typed capability to send buffer
			 */
			template <typename IT>
			Ipc_ostream &operator << (Capability<IT> const &typed_cap)
			{
				_marshal_capability(typed_cap);
				return *this;
			}

			/**
			 * Issue the sending of the message buffer
			 */
			Ipc_ostream &operator << (Ipc_ostream_send)
			{
				_send();
				return *this;
			}

			/**
			 * Return current 'IPC_SEND' destination
			 *
			 * This function is typically needed by a server than sends replies
			 * in a different order as the incoming calls.
			 */
			Native_capability dst() const { return _dst; }

			/**
			 * Set destination for the next 'IPC_SEND'
			 */
			void dst(Native_capability const &dst) { _dst = dst; }
	};


	/**
	 * Stream for receiving information
	 */
	class Ipc_istream : public Ipc_unmarshaller, public Native_capability
	{
		private:

			/**
			 * Prevent 'Ipc_istream' objects from being copied
			 *
			 * Copying an 'Ipc_istream' object would result in a duplicated
			 * (and possibly inconsistent) connection state both the original
			 * and the copied object.
			 */
			Ipc_istream(const Ipc_istream &);

		protected:

			Msgbuf_base            *_rcv_msg;
			Native_connection_state _rcv_cs;

			/**
			 * Obtain capability from message buffer
			 */
			void _unmarshal_capability(Native_capability &cap);

		protected:

			/**
			 * Reset unmarshaller
			 */
			void _prepare_next_receive();

			/**
			 * Wait for incoming message to be received in _rcv_msg
			 */
			void _wait();

		public:

			explicit Ipc_istream(Msgbuf_base *rcv_msg);

			~Ipc_istream();

			/**
			 * Read badge that was supplied with the message
			 */
			long badge() { return _long_at_idx(0); }

			/**
			 * Block for an incoming message filling the receive buffer
			 */
			Ipc_istream &operator >> (Ipc_istream_wait)
			{
				_wait();
				return *this;
			}

			/**
			 * Read values from receive buffer
			 */
			template <typename T>
			Ipc_istream &operator >> (T &value)
			{
				_read_from_buf(value);
				return *this;
			}

			/**
			 * Read byte buffer from receive buffer
			 */
			Ipc_istream &operator >> (Rpc_in_buffer_base &b)
			{
				_read_bytebuf_from_buf(b);
				return *this;
			}

			/**
			 * Read byte buffer from receive buffer
			 */
			template <size_t MAX_BUFFER_SIZE>
			Ipc_istream &operator >> (Rpc_in_buffer<MAX_BUFFER_SIZE> &b)
			{
				_read_bytebuf_from_buf(b);
				return *this;
			}

			/**
			 * Read capability from receive buffer
			 */
			Ipc_istream &operator >> (Native_capability &cap)
			{
				_unmarshal_capability(cap);
				return *this;
			}

			/**
			 * Read typed capability from receive buffer
			 */
			template <typename IT>
			Ipc_istream &operator >> (Capability<IT> &typed_cap)
			{
				_unmarshal_capability(typed_cap);
				return *this;
			}
	};


	class Ipc_client: public Ipc_istream, public Ipc_ostream
	{
		protected:

			int _result;   /* result of most recent call */

			void _prepare_next_call();

			/**
			 * Send RPC message and wait for result
			 */
			void _call();

			/**
			 * Set return value if call to server failed
			 */
			void ret(int retval)
			{
				*reinterpret_cast<int *>(&_rcvbuf[sizeof(umword_t)]) = retval;
			}

		public:

			/**
			 * Constructor
			 */
			Ipc_client(Native_capability const &srv, Msgbuf_base *snd_msg,
			                                         Msgbuf_base *rcv_msg);

			/**
			 * Operator that issues an IPC call
			 *
			 * \throw Ipc_error
			 * \throw Blocking_canceled
			 */
			Ipc_client &operator << (Ipc_client_call)
			{
				_call();
				_read_from_buf(_result);
				if (_result == ERR_INVALID_OBJECT) {
					PERR("tried to call an invalid object");
					throw Ipc_error();
				}
				return *this;
			}

			template <typename T>
			Ipc_client &operator << (T const &value)
			{
				_write_to_buf(value);
				return *this;
			}

			Ipc_client &operator << (Rpc_in_buffer_base const &b)
			{
				_write_buffer_to_buf(b);
				return *this;
			}

			template <size_t MAX_BUFFER_SIZE>
			Ipc_client &operator << (Rpc_in_buffer<MAX_BUFFER_SIZE> const &b)
			{
				_write_buffer_to_buf(b);
				return *this;
			}

			Ipc_client &operator << (Native_capability const &cap)
			{
				_marshal_capability(cap);
				return *this;
			}

			template <typename IT>
			Ipc_client &operator << (Capability<IT> const &typed_cap)
			{
				_marshal_capability(typed_cap);
				return *this;
			}

			Ipc_client &operator >> (Native_capability &cap)
			{
				_unmarshal_capability(cap);
				return *this;
			}

			template <typename IT>
			Ipc_client &operator >> (Capability<IT> &typed_cap)
			{
				_unmarshal_capability(typed_cap);
				return *this;
			}

			template <typename T>
			Ipc_client &operator >> (T &value)
			{
				_read_from_buf(value);
				return *this;
			}

			Ipc_client &operator >> (Rpc_in_buffer_base &b)
			{
				_read_bytebuf_from_buf(b);
				return *this;
			}

			int result() const { return _result; }
	};


	class Ipc_server : public Ipc_istream, public Ipc_ostream
	{
		protected:

			bool _reply_needed;   /* false for the first reply_wait */

			void _prepare_next_reply_wait();

			/**
			 * Wait for incoming call
			 *
			 * In constrast to 'Ipc_istream::_wait()', this function stores the
			 * next reply destination from into 'dst' of the 'Ipc_ostream'.
			 */
			void _wait();

			/**
			 * Send reply to destination
			 *
			 * In contrast to 'Ipc_ostream::_send()', this function prepares
			 * the 'Ipc_server' to send another subsequent reply without the
			 * calling '_wait()' in between. This is needed when a server
			 * answers calls out of order.
			 */
			void _reply();

			/**
			 * Send result of previous RPC request and wait for new one
			 */
			void _reply_wait();

		public:

			/**
			 * Constructor
			 */
			Ipc_server(Msgbuf_base *snd_msg, Msgbuf_base *rcv_msg);

			/**
			 * Set return value of server call
			 */
			void ret(int retval)
			{
				*reinterpret_cast<int *>(&_sndbuf[sizeof(umword_t)]) = retval;
			}

			/**
			 * Set reply destination
			 */
			void dst(Native_capability const &reply_dst)
			{
				Ipc_ostream::dst(reply_dst);
				_reply_needed = reply_dst.valid();
			}

			using Ipc_ostream::dst;

			/**
			 * Block for an incoming message filling the receive buffer
			 */
			Ipc_server &operator >> (Ipc_istream_wait)
			{
				_wait();
				return *this;
			}

			/**
			 * Issue the sending of the message buffer
			 */
			Ipc_server &operator << (Ipc_server_reply)
			{
				_reply();
				return *this;
			}

			/**
			 * Reply current request and wait for a new one
			 */
			Ipc_server &operator >> (Ipc_server_reply_wait)
			{
				_reply_wait();
				return *this;
			}

			/**
			 * Write value to send buffer
			 *
			 * This operator is only used by test programs
			 */
			template <typename T>
			Ipc_server &operator << (T const &value)
			{
				_write_to_buf(value);
				return *this;
			}

			/**
			 * Read value from receive buffer
			 *
			 * This operator should only be used by the server framework for
			 * reading the function offset. The server-side processing of the
			 * payload is done using 'Ipc_istream' and 'Ipc_ostream'.
			 */
			template <typename T>
			Ipc_server &operator >> (T &value)
			{
				_read_from_buf(value);
				return *this;
			}
	};
}

#endif /* _INCLUDE__BASE__IPC_GENERIC_H_ */
