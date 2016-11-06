/*
 * \brief  Common types used within the linker
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TYPES_H_
#define _INCLUDE__TYPES_H_

#include <base/exception.h>
#include <base/env.h>
#include <base/shared_object.h>
#include <util/volatile_object.h>
#include <util/fifo.h>
#include <util/misc_math.h>
#include <util/string.h>

namespace Linker {

	using namespace Genode;

	/**
	 * Exceptions
	 */
	class Incompatible : Exception  { };
	class Invalid_file : Exception  { };
	class Not_found    : Exception  { };

	enum Keep { DONT_KEEP = Shared_object::DONT_KEEP,
	            KEEP      = Shared_object::KEEP };

	enum Bind { BIND_LAZY = Shared_object::BIND_LAZY,
	            BIND_NOW  = Shared_object::BIND_NOW };

	/**
	 * Invariants
	 */
	constexpr char const *binary_name() { return "binary"; }
	constexpr char const *linker_name() { return "ld.lib.so"; }
}

#endif /* _INCLUDE__TYPES_H_ */
