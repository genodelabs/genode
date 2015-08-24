/*
 * \brief  RM- and pager implementations specific for base-hw and core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-02-12
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <pager.h>
#include <rm_session_component.h>
#include <platform.h>
#include <platform_pd.h>
#include <platform_thread.h>
#include <translation_table.h>

using namespace Genode;


/***************
 ** Rm_client **
 ***************/

void Rm_client::unmap(addr_t, addr_t virt_base, size_t size)
{
	Locked_ptr<Address_space> locked_address_space(_address_space);

	if (locked_address_space.is_valid())
		locked_address_space->flush(virt_base, size);
}


/**********************
 ** Pager_entrypoint **
 **********************/

int Pager_entrypoint::apply_mapping()
{
	Page_flags const flags =
	Page_flags::apply_mapping(_mapping.writable,
	                          _mapping.cacheable,
	                          _mapping.io_mem);
	Platform_pd * const pd = (Platform_pd*)_fault.pd;

	return (pd->insert_translation(_mapping.virt_address,
	                               _mapping.phys_address,
	                               1 << _mapping.size_log2, flags)) ? 0 : 1;
}


void Pager_entrypoint::entry()
{
	while (1)
	{
		/* receive fault */
		if (Kernel::await_signal(_cap.dst())) continue;
		Pager_object * po =
			*(Pager_object**)Thread_base::myself()->utcb()->base();

		/*
		 * Synchronize access and ensure that the object is still managed
		 *
		 * FIXME: The implicit lookup of the oject isn't needed.
		 */
		unsigned const pon = po->cap().local_name();
		Object_pool<Pager_object>::Guard pog(lookup_and_lock(pon));
		if (!pog) continue;

		/* fetch fault data */
		Platform_thread * const pt = (Platform_thread *)pog->badge();
		if (!pt) {
			PWRN("failed to get platform thread of faulter");
			continue;
		}

		_fault.pd     = pt->kernel_object()->fault_pd();
		_fault.ip     = pt->kernel_object()->ip;
		_fault.addr   = pt->kernel_object()->fault_addr();
		_fault.writes = pt->kernel_object()->fault_writes();
		_fault.signal = pt->kernel_object()->fault_signal();

		/* try to resolve fault directly via local region managers */
		if (pog->pager(*this)) { continue; }

		/* apply mapping that was determined by the local region managers */
		if (apply_mapping()) {
			PWRN("failed to apply mapping");
			continue;
		}
		/* let pager object go back to no-fault state */
		pog->wake_up();
	}
}
