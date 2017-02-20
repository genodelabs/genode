/*
 * \brief  Kernel-specific RM-faulter wake-up mechanism
 * \author Norman Feske
 * \date   2016-04-12
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pager.h>

/* base-internal includes */
#include <base/internal/cap_map.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
}

using namespace Genode;


void Pager_object::wake_up()
{
	using namespace Fiasco;

	/*
	 * Issue IPC to pager, transmitting the pager-object pointer as 'IP'.
	 */
	l4_utcb_mr()->mr[0] = 0;                 /* fault address */
	l4_utcb_mr()->mr[1] = (l4_umword_t)this; /* instruction pointer */

	l4_ipc_call(cap().data()->kcap(), l4_utcb(), l4_msgtag(0, 2, 0, 0), L4_IPC_NEVER);
}


void Pager_object::unresolved_page_fault_occurred()
{
	state.unresolved_page_fault = true;
}
