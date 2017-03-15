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

namespace Linker {

	using namespace Genode;

	/**
	 * Exceptions
	 */
	class Incompatible : Exception  { };
	class Invalid_file : Exception  { };
	class Not_found    : Exception  { };
	class Fatal        : Exception  { };

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
