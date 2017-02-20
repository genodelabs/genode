/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/lock.h>

/* L4/Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}


void Genode::sleep_forever()
{
	while (true) Fiasco::l4_ipc_sleep((Fiasco::l4_timeout_t){0});
}
