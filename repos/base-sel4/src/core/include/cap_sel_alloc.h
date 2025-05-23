/*
 * \brief   Interface for capability-selector allocator
 * \author  Norman Feske
 * \date    2015-05-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CAP_SEL_ALLOC_H_
#define _CORE__INCLUDE__CAP_SEL_ALLOC_H_

/* Genode includes */
#include <base/exception.h>
#include <util/bit_allocator.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

/* core includes */
#include <core_cspace.h>
#include <types.h>

namespace Core { struct Cap_sel_alloc; }


struct Core::Cap_sel_alloc : Interface
{
	struct Alloc_failed : Exception { };

	using Core_sel_bit_alloc = Bit_allocator<1 << Core_cspace::NUM_CORE_SEL_LOG2>;
	using Cap_sel_error      = Core_sel_bit_alloc::Error;
	using Cap_sel_attempt    = Attempt<addr_t, Cap_sel_error>;

	virtual Cap_sel_attempt alloc() = 0;

	virtual void free(Cap_sel) = 0;
};

#endif /* _CORE__INCLUDE__CAP_SEL_ALLOC_H_ */
