/*
 * \brief  Fiasco-specific implementation of the thread API
 * \author Norman Feske
 * \date   2016-01-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/native_utcb.h>

using namespace Genode;


Native_utcb *Thread_base::utcb()
{
	return &_stack->utcb();
}
