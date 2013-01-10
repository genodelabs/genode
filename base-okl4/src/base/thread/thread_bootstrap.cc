/*
 * \brief  Default thread bootstrap code
 * \author Norman Feske
 * \date   2009-04-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>

namespace Okl4 { extern "C" {
#include <l4/utcb.h>
#include <l4/thread.h>
} }

namespace Okl4 {
	extern L4_Word_t copy_uregister_to_utcb(void);
}


void Genode::Thread_base::_thread_bootstrap()
{
	_tid.l4id.raw = Okl4::copy_uregister_to_utcb();
}
