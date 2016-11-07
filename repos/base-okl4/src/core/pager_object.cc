/*
 * \brief  Kernel-specific RM-faulter wake-up mechanism
 * \author Norman Feske
 * \date   2016-04-12
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pager.h>

/* base-internal includes */
#include <base/internal/capability_space_tpl.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/message.h>
#include <l4/ipc.h>
} }

using namespace Genode;


void Pager_object::wake_up()
{
	using namespace Okl4;

	/*
	 * Issue IPC to pager, transmitting the pager-object pointer as 'IP'.
	 */
	L4_Accept(L4_UntypedWordsAcceptor);

	L4_MsgTag_t snd_tag;
	snd_tag.raw = 0;
	snd_tag.X.u = 2;

	L4_LoadMR(0, snd_tag.raw);
	L4_LoadMR(1, 0);                    /* fault address */
	L4_LoadMR(2, (unsigned long)this);  /* instruction pointer */

	L4_Call(Capability_space::ipc_cap_data(cap()).dst);
}


void Pager_object::unresolved_page_fault_occurred()
{
	state.unresolved_page_fault = true;
}
