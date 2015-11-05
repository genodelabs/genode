/*
 * \brief  Pci device protection domain service for platform driver 
 * \author Alexander Boettcher
 * \date   2013-02-10
 */

/*
 * Copyright (C) 2013-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/static_root.h>
#include <base/printf.h>
#include <base/sleep.h>

#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <pd_session/client.h>

#include <util/flex_iterator.h>

#include "../pci_device_pd_ipc.h"

static bool map_eager(Genode::addr_t const page, unsigned log2_order)
{
	using Genode::addr_t;

	Genode::Thread_base * myself = Genode::Thread_base::myself();
	Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());
	Nova::Rights const mapping_rw(true, true, false);

	addr_t const page_fault_portal = myself->tid().exc_pt_sel + 14;

	/* setup faked page fault information */
	utcb->set_msg_word(((addr_t)&utcb->qual[2] - (addr_t)utcb->msg) / sizeof(addr_t));
	utcb->ip      = reinterpret_cast<addr_t>(map_eager);
	utcb->qual[1] = page;
	utcb->crd_rcv = Nova::Mem_crd(page >> 12, log2_order - 12, mapping_rw);

	/* trigger faked page fault */
	Genode::uint8_t res = Nova::call(page_fault_portal);
	return res == Nova::NOVA_OK;
}

void Platform::Device_pd_component::attach_dma_mem(Genode::Dataspace_capability ds_cap)
{
	using namespace Genode;

	Dataspace_client ds_client(ds_cap);

	addr_t const phys = ds_client.phys_addr();
	size_t const size = ds_client.size();

	addr_t page = ~0UL;

	try {
		page = env()->rm_session()->attach_at(ds_cap, phys);
	} catch (...) { }

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			env()->rm_session()->detach(page);

		PERR("attachment of DMA memory @ %lx+%zx failed", phys, size);
		return;
	}

	Genode::Flexpage_iterator it(page, size, page, size, 0);
	for (Genode::Flexpage flex = it.page(); flex.valid(); flex = it.page()) {
		if (map_eager(flex.addr, flex.log2_order))
			continue;

		PERR("attachment of DMA memory @ %lx+%zx failed at %lx", phys, size,
		     flex.addr);
		return;
	}
}

void Platform::Device_pd_component::assign_pci(Genode::Io_mem_dataspace_capability io_mem_cap)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = env()->rm_session()->attach(io_mem_cap);
	/* sanity check */
	if (!page)
		throw Rm_session::Region_conflict();

	/* trigger mapping of whole memory area */
	if (!map_eager(page, 12))
		PERR("assignment of PCI device failed - %lx", page);

	/* try to assign pci device to this protection domain */
	if (!env()->pd_session()->assign_pci(page))
		PERR("assignment of PCI device failed");

	/* we don't need the mapping anymore */
	env()->rm_session()->detach(page);
}

int main(int argc, char **argv)
{
	using namespace Genode;

	Genode::printf("Device protection domain starting ...\n");

	/*
	 * Initialize server entry point
	 */
	enum {
		STACK_SIZE       = 1024*sizeof(Genode::addr_t)
	};

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "device_pd_ep");

	static Platform::Device_pd_component pd_component;

	/*
	 * Attach input root interface to the entry point
	 */
	static Static_root<Platform::Device_pd> root(ep.manage(&pd_component));

	env()->parent()->announce(ep.manage(&root));

	printf("Device protection domain started\n");

	Genode::sleep_forever();
	return 0;
}
