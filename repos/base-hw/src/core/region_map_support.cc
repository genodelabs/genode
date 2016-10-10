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
	while (1)
	{
		/* receive fault */
		if (Kernel::await_signal(Capability_space::capid(_cap))) continue;

		Untyped_capability cap =
			(*(Pager_object**)Thread::myself()->utcb()->data())->cap();

		/*
		 * Synchronize access and ensure that the object is still managed
		 *
		 * FIXME: The implicit lookup of the oject isn't needed.
		 */
		auto lambda = [&] (Pager_object *po) {
			if (!po) return;

			/* fetch fault data */
			Platform_thread * const pt = (Platform_thread *)po->badge();
			if (!pt) {
				Genode::warning("failed to get platform thread of faulter");
				return;
			}

			_fault.ip     = pt->kernel_object()->ip;
			_fault.addr   = pt->kernel_object()->fault_addr();
			_fault.writes = pt->kernel_object()->fault_writes();
			_fault.signal = pt->kernel_object()->fault_signal();

			/* try to resolve fault directly via local region managers */
			if (po->pager(*this)) return;

			/* apply mapping that was determined by the local region managers */
			{
				Locked_ptr<Address_space> locked_ptr(pt->address_space());
				if (!locked_ptr.valid()) return;

				Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);
				Page_flags const flags =
					Page_flags::apply_mapping(_mapping.writable,
											  _mapping.cacheable,
											  _mapping.io_mem);
				as->insert_translation(_mapping.virt_address,
									   _mapping.phys_address,
									   1 << _mapping.size_log2, flags);
			}

			/* let pager object go back to no-fault state */
			po->wake_up();
		};
		apply(cap, lambda);
	}
}
