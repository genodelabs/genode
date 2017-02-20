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
#include <base/internal/capability_space_tpl.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
}

using namespace Genode;


void Pager_object::wake_up()
{
	using namespace Fiasco;

	/* kernel-defined message header */
	struct {
		l4_fpage_t   rcv_fpage; /* unused */
		l4_msgdope_t size_dope = L4_IPC_DOPE(0, 0);
		l4_msgdope_t send_dope = L4_IPC_DOPE(0, 0);
	} rcv_header;

	l4_msgdope_t ipc_result;
	l4_umword_t dummy = 0;
	l4_ipc_call(Capability_space::ipc_cap_data(cap()).dst, L4_IPC_SHORT_MSG,
	            0,                 /* fault address */
	            (l4_umword_t)this, /* instruction pointer */
	            &rcv_header, &dummy, &dummy, L4_IPC_NEVER, &ipc_result);
}


void Pager_object::unresolved_page_fault_occurred()
{
	state.unresolved_page_fault = true;
}
