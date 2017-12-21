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

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>

namespace Genode { struct Cap_sel_alloc; }


struct Genode::Cap_sel_alloc : Interface
{
	struct Alloc_failed : Exception { };

	virtual Cap_sel alloc() = 0;

	virtual void free(Cap_sel) = 0;
};

#endif /* _CORE__INCLUDE__CAP_SEL_ALLOC_H_ */
