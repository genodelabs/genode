/*
 * \brief  Thread bootstrap code
 * \author Christian Prochaska
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>

/* Pistachio includes */
namespace Pistachio {
#include <l4/thread.h>
}

void Genode::Thread_base::_thread_bootstrap()
{
	_tid.l4id = Pistachio::L4_Myself();
}
