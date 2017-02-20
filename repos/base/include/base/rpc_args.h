/*
 * \brief  Helpers for non-ordinary RPC arguments
 * \author Norman Feske
 * \date   2011-04-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RPC_ARGS_H_
#define _INCLUDE__BASE__RPC_ARGS_H_

#include <util/string.h>
#include <base/stdint.h>

namespace Genode {

	class Rpc_in_buffer_base;
	template <size_t> class Rpc_in_buffer;
}


/**
 * Base class of 'Rpc_in_buffer'
 */
class Genode::Rpc_in_buffer_base
{
	protected:

		const char *_base;
		size_t      _size;

		/**
		 * Construct buffer from null-terminated string
		 */
		explicit Rpc_in_buffer_base(const char *str)
		: _base(str), _size(strlen(str) + 1) { }

		/**
		 * Construct an empty buffer by default
		 */
		Rpc_in_buffer_base(): _base(0), _size(0) { }

	public:

		/**
		 * Construct buffer
		 */
		Rpc_in_buffer_base(const char *base, size_t size)
		: _base(base), _size(size) { }

		const char *base() const { return _base; }
		size_t      size() const { return _size; }
};


/**
 * Buffer with size constrain
 */
template <Genode::size_t MAX>
class Genode::Rpc_in_buffer : public Rpc_in_buffer_base
{
	private:

		/*
		 * This member is only there to pump up the size of the object such
		 * that 'sizeof()' returns the maximum buffer size when queried by
		 * the RPC framework.
		 */
		char _balloon[MAX];

	public:

		enum { MAX_SIZE = MAX };

		/**
		 * Construct buffer
		 */
		Rpc_in_buffer(const char *base, size_t size)
		: Rpc_in_buffer_base(base, min(size, (size_t)MAX_SIZE)) { }

		/**
		 * Construct buffer from null-terminated string
		 */
		Rpc_in_buffer(const char *str) : Rpc_in_buffer_base(str)
		{
			if (_size >= MAX_SIZE)
				_size = MAX_SIZE;
		}

		/**
		 * Default constructor creates invalid buffer
		 */
		Rpc_in_buffer() { }

		void operator = (Rpc_in_buffer<MAX_SIZE> const &from)
		{
			_base = from.base();
			_size = from.size();
		}

		/**
		 * Return true if buffer contains a valid null-terminated string
		 */
		bool valid_string() const {
			return (_size <= MAX_SIZE) && (_size > 0) && (_base[_size - 1] == '\0'); }

		/**
		 * Return true if buffer contains a valid null-terminated string
		 *
		 * \noapi
		 * \deprecated use valid_string instead
		 */
		bool is_valid_string() const { return valid_string(); }

		/**
		 * Return buffer content as null-terminated string
		 *
		 * \return  pointer to null-terminated string
		 *
		 * The method returns an empty string if the buffer does not hold
		 * a valid null-terminated string. To distinguish a buffer holding
		 * an invalid string from a buffer holding a valid empty string,
		 * the function 'valid_string' can be used.
		 */
		char const *string() const { return valid_string() ? base() : ""; }
};

#endif /* _INCLUDE__BASE__RPC_ARGS_H_ */
