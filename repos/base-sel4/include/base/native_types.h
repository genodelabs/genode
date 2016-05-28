/*
 * \brief  Platform-specific type definitions
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_TYPES_H_
#define _INCLUDE__BASE__NATIVE_TYPES_H_

#include <base/stdint.h>
#include <base/native_capability.h>

namespace Genode {

	class Native_capability
	{
		public:

			/*
			 * XXX remove dependency in 'process.cc' and 'core_env.h' from
			 * 'Raw', 'Dst', and the 'dst' member.
			 */
			typedef int Dst;
			struct Raw { Dst dst = 0; long local_name = 0; };
			Dst dst() const { return 0; }

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
			Native_capability() { }

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
			Native_capability(Data &data) : _data(&data) { _inc(); }

			/**
			 * Destructor
			 */
			~Native_capability() { _dec(); }

			Data const *data() const { return _data; }

			/**
			 * Overloaded comparision operator
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
	};
}

#endif /* _INCLUDE__BASE__NATIVE_TYPES_H_ */
