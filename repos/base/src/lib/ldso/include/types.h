/*
 * \brief  Common types used within the linker
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TYPES_H_
#define _INCLUDE__TYPES_H_

#include <base/exception.h>
#include <base/env.h>
#include <base/shared_object.h>
#include <util/reconstructible.h>
#include <util/fifo.h>
#include <util/misc_math.h>
#include <util/string.h>
#include <base/internal/globals.h>

namespace Linker {

	using namespace Genode;

	/**
	 * Exceptions
	 */
	class Incompatible : Exception  { };
	class Invalid_file : Exception  { };
	class Fatal        : Exception  { };

	class Not_found    : Exception
	{
		private:

			enum { CAPACITY = 128 };
			char _buf[CAPACITY];

		public:

			Not_found() : _buf("<unkown>") { }
			Not_found(char const *str)
			{
				cxx_demangle(str, _buf, CAPACITY);
			}

			void print(Output &out) const
			{
				Genode::print(out, Cstring(_buf, CAPACITY));
			}
	};

	class Current_exception
	{
		private:

			enum { CAPACITY = 128 };
			char _buf[CAPACITY];

		public:

			Current_exception() : _buf("<unkown>")
			{
				cxx_current_exception(_buf, CAPACITY);
			}

			void print(Output &out) const
			{
				Genode::print(out, Cstring(_buf, CAPACITY));
			}
	};

	enum Keep { DONT_KEEP = Shared_object::DONT_KEEP,
	            KEEP      = Shared_object::KEEP };

	enum Bind { BIND_LAZY = Shared_object::BIND_LAZY,
	            BIND_NOW  = Shared_object::BIND_NOW };

	enum Stage { STAGE_BINARY, STAGE_SO };

	/**
	 * Invariants
	 */
	constexpr char const *binary_name() { return "binary"; }
	constexpr char const *linker_name() { return "ld.lib.so"; }
}

#endif /* _INCLUDE__TYPES_H_ */
