/*
 * \brief  Pager support for Microblaze Kernel
 * \author Norman Feske
 * \author Martin Stein
 * \date   2010-09-23
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/ipc_pager.h>
#include <base/printf.h>

/* kernel includes */
#include <kernel/syscalls.h>

using namespace Genode;


void Ipc_pager::wait_for_fault()
{
	typedef Kernel::Paging::Request Request;

	while (1) {

		/* wait for fault message */
		unsigned const msg_length=Kernel::ipc_serve(0);

		/* check message format */
		if (msg_length==sizeof(Request)){

			_request=*((Request*)Thread_base::myself()->utcb());

//			PERR(
//				"Recieved pagefault, va=%p, tid=%i, pid=%i",
//				_request.virtual_page.address(),
//				_request.source.tid,
//				_request.virtual_page.protection_id());

			return;
		}
	}
}


void Ipc_pager::reply_and_wait_for_fault()
{
	/* load mapping to tlb (not to be considered permanent) */
	if (_mapping.valid())
		Kernel::tlb_load(
			_mapping.physical_page.address(),
			_mapping.virtual_page.address(),
			_request.virtual_page.protection_id(),
			_mapping.physical_page.size(),
			_mapping.physical_page.permissions());

//	PERR(
//		"Resoluted, pa=%p, va=%p, tid=%i, pid=%i",
//		_mapping.physical_page.address(),
//		_mapping.virtual_page.address(),
//		_request.source.tid,
//		_request.virtual_page.protection_id());

	/* wake up faulter if mapping succeeded */ acknowledge_wakeup();

	/* wait for next page fault */ wait_for_fault();
}


