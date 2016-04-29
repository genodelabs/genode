/*
 * \brief  Implementation of the Thread API (foc-specific myself())
 * \author Norman Feske
 * \date   2015-04-28
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>


Genode::Thread_base *Genode::Thread_base::myself()
{
	using namespace Fiasco;

	return reinterpret_cast<Thread_base*>(l4_utcb_tcr()->user[UTCB_TCR_THREAD_OBJ]);
}


