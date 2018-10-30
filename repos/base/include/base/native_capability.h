/*
 * \brief  Capability type
 * \author Norman Feske
 * \date   2016-07-15
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

#include <base/stdint.h>
#include <base/output.h>
#include <base/exception.h>

namespace Genode { class Native_capability; }


class Genode::Native_capability
{
	public:

		struct Reference_count_overflow : Exception { };

		/*
		 * Platform-specific raw information of the capability that is
		 * transferred as-is when the capability is delegated.
		 */
		struct Raw { unsigned long v[4]; };

		/**
		 * Forward declaration of the platform-specific internal capability
		 * representation
		 */
		class Data;

	private:

		Data *_data = nullptr;

	protected:

		void _inc();
		void _dec();

	public:

		/**
		 * Default constructor creates an invalid capability
		 */
		Native_capability();

		/**
		 * Copy constructor
		 */
		Native_capability(const Native_capability &other)
		: _data(other._data) { _inc(); }

		/**
		 * Construct capability manually
		 *
		 * This constructor is used internally.
		 *
		 * \noapi
		 */
		Native_capability(Data *data) : _data(data) { _inc(); }

		/**
		 * Destructor
		 */
		~Native_capability() { _dec(); }

		Data const *data() const { return _data; }

		/**
		 * Overloaded comparison operator
		 */
		bool operator == (const Native_capability &o) const
		{
			return _data == o._data;
		}

		Native_capability& operator = (const Native_capability &o)
		{
			if (this == &o)
				return *this;

			_dec();
			_data = o._data;
			_inc();
			return *this;
		}

		long local_name() const;

		bool valid() const;

		Raw raw() const;

		void print(Output &) const;
};

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */
