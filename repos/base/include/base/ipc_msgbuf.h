/*
 * \brief  IPC message buffer layout
 * \author Norman Feske
 * \date   2015-05-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__IPC_MSGBUF_H_
#define _INCLUDE__BASE__IPC_MSGBUF_H_

#include <util/noncopyable.h>
#include <base/capability.h>
#include <base/exception.h>
#include <base/rpc_args.h>

namespace Genode {

	class Msgbuf_base;
	template <unsigned> class Msgbuf;
}


class Genode::Msgbuf_base : Noncopyable
{
	public:

		static constexpr Genode::size_t MAX_CAPS_PER_MSG = 4;

	private:

		/*
		 * Resolve ambiguity if the header is included from a libc-using
		 * program.
		 */
		typedef Genode::size_t size_t;

		/*
		 * Capabilities to be transferred
		 */
		Native_capability _caps[MAX_CAPS_PER_MSG];

		/**
		 * Number of marshalled capabilities
		 */
		size_t _used_caps = 0;

		/**
		 * Pointer to buffer for data payload
		 */
		char * const _data;

		/**
		 * Maximum size of plain-data message payload
		 */
		size_t const _capacity;

		/**
		 * Actual size of plain-data message payload
		 */
		size_t _data_size = 0;

		char *_data_last() { return &_data[_data_size]; }

		void _clear(size_t const num_bytes)
		{
			size_t const num_words = min(num_bytes, _capacity)/sizeof(long);
			for (unsigned i = 0; i < num_words; i++)
				word(i) = 0;
		}

		/*
		 * Noncopyable
		 */
		Msgbuf_base(Msgbuf_base const &);
		Msgbuf_base &operator = (Msgbuf_base const &);

	protected:

		struct Headroom { long space[16]; };

		Msgbuf_base(char *buf, size_t capacity)
		:
			_data(buf), _capacity(capacity)
		{
			_clear(capacity);
		}

	public:

		/**
		 * Return reference to platform-specific header in front of the message
		 */
		template <typename T>
		T &header()
		{
			static_assert(sizeof(T) <= sizeof(Headroom),
			              "Header size exceeds message headroom");
			return *reinterpret_cast<T *>(_data - sizeof(T));
		}

		/**
		 * Return reference to the message word at the specified index
		 */
		unsigned long &word(unsigned i)
		{
			return reinterpret_cast<unsigned long *>(_data)[i];
		}

		/**
		 * Return size of message buffer
		 */
		size_t capacity() const { return _capacity; }

		/**
		 * Reset message buffer
		 *
		 * This function is used at the server side for reusing the same
		 * message buffer for subsequent requests.
		 */
		void reset()
		{
			for (Genode::size_t i = 0; i < _used_caps; i++)
				_caps[i] = Native_capability();

			_clear(_data_size);

			_used_caps = 0;
			_data_size = 0;
		}

		/**
		 * Return pointer to start of message-buffer content
		 */
		void const *data() const { return _data; }
		void       *data()       { return _data; }

		/**
		 * Return size of marshalled data payload in bytes
		 */
		size_t data_size() const { return _data_size; }

		void data_size(size_t data_size) { _data_size = data_size; }

		/**
		 * Exception type
		 */
		class Too_many_caps : public Exception { };

		/**
		 * Return number of marshalled capabilities
		 */
		size_t used_caps() const { return _used_caps; }

		void used_caps(size_t used_caps) { _used_caps = used_caps; }

		Native_capability       &cap(unsigned i)       { return _caps[i]; }
		Native_capability const &cap(unsigned i) const { return _caps[i]; }

		/**
		 * Append value to message buffer
		 */
		template <typename T>
		void insert(T const &value)
		{
			/* check buffer range */
			if (_data_size + sizeof(T) > _capacity) return;

			/* write value to buffer */
			*reinterpret_cast<T *>(_data_last()) = value;

			/* increment write pointer to next dword-aligned value */
			_data_size += align_natural(sizeof(T));
		}

		/**
		 * Insert content of 'Rpc_in_buffer' into message buffer
		 */
		template <size_t MAX_BUFFER_SIZE>
		void insert(Rpc_in_buffer<MAX_BUFFER_SIZE> const &b)
		{
			size_t const size = b.size();
			insert(size);
			insert(b.base(), size);
		}

		/**
		 * Write bytes to message buffer
		 */
		void insert(char const *src_addr, unsigned num_bytes)
		{
			/* check buffer range */
			if (_data_size + num_bytes > _capacity) return;

			/* copy buffer */
			memcpy(_data_last(), src_addr, num_bytes);

			/* increment write pointer to next dword-aligned value */
			_data_size += align_natural(num_bytes);
		}

		/**
		 * Insert capability to message buffer
		 */
		void insert(Native_capability const &cap)
		{
			if (_used_caps == MAX_CAPS_PER_MSG)
				throw Too_many_caps();

			_caps[_used_caps++] = cap;
		}

		/**
		 * Insert typed capability into message buffer
		 */
		template <typename IT>
		void insert(Capability<IT> const &typed_cap)
		{
			Native_capability untyped_cap = typed_cap;
			insert(untyped_cap);
		}
};


template <unsigned BUF_SIZE>
struct Genode::Msgbuf : Msgbuf_base
{
	/**
	 * Headroom in front of the actual message payload
	 *
	 * This space is used on some platforms to prepend the message with a
	 * protocol header.
	 */
	Headroom headroom { };

	/**
	 * Buffer for data payload
	 */
	char buf[BUF_SIZE];

	Msgbuf() : Msgbuf_base(buf, BUF_SIZE) { }
};

#endif /* _INCLUDE__BASE__IPC_MSGBUF_H_ */
