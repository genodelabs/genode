/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pager.h>
#include <rm_session_component.h>
#include <platform.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <translation_table.h>

/* base-internal includes */

using namespace Genode;


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.valid())
		locked_address_space->flush(virt_base, size);
}


/**********************
 ** Pager_entrypoint **
 **********************/

void Pager_entrypoint::entry()
{
	Untyped_capability cap;

	while (1)
	{
		if (cap.valid()) Kernel::ack_signal(Capability_space::capid(cap));

		/* receive fault */
		if (Kernel::await_signal(Capability_space::capid(_cap))) continue;

		Pager_object *po = *(Pager_object**)Thread::myself()->utcb()->data();
		cap = po->cap();

		if (!po) continue;

		/* fetch fault data */
		Platform_thread * const pt = (Platform_thread *)po->badge();
		if (!pt) {
			Genode::warning("failed to get platform thread of faulter");
			continue;
		}

		_fault.ip     = pt->kernel_object()->ip;
		_fault.addr   = pt->kernel_object()->fault_addr();
		_fault.writes = pt->kernel_object()->fault_writes();

		/* try to resolve fault directly via local region managers */
		if (po->pager(*this)) continue;

		/* apply mapping that was determined by the local region managers */
		{
			Locked_ptr<Address_space> locked_ptr(pt->address_space());
			if (!locked_ptr.valid()) continue;

			Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);
			as->insert_translation(_mapping.virt(), _mapping.phys(),
			                       _mapping.size(), _mapping.flags());
		}

		/* let pager object go back to no-fault state */
		po->wake_up();
	}
}
