/*
 * \brief  Implementation of the Thread API (foc-specific myself())
 * \author Norman Feske
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>


Genode::Thread *Genode::Thread::myself()
{
	using namespace Fiasco;

	return reinterpret_cast<Thread*>(l4_utcb_tcr()->user[UTCB_TCR_THREAD_OBJ]);
}


