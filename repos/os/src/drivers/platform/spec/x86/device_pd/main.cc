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
#include <rm_session/client.h>
#include <pd_session/client.h>

#include <util/flex_iterator.h>
#include <util/retry.h>

#include "../pci_device_pd_ipc.h"

/**
 *
 */
struct Expanding_rm_session_client : Genode::Rm_session_client
{
	Genode::Rm_session_capability _cap;

	Expanding_rm_session_client(Genode::Rm_session_capability cap)
	: Rm_session_client(cap), _cap(cap) { }

	Local_addr attach(Genode::Dataspace_capability ds,
	                          Genode::size_t size, Genode::off_t offset,
	                          bool use_local_addr,
	                          Local_addr local_addr,
	                          bool executable) override
	{
		return Genode::retry<Rm_session::Out_of_metadata>(
			[&] () {
				return Rm_session_client::attach(ds, size, offset,
				                                 use_local_addr,
				                                 local_addr,
				                                 executable); },
			[&] () {
				enum { UPGRADE_QUOTA = 4096 };

				if (Genode::env()->ram_session()->avail() < UPGRADE_QUOTA)
					throw;

				char buf[32];
				Genode::snprintf(buf, sizeof(buf), "ram_quota=%u",
				                 UPGRADE_QUOTA);

				Genode::env()->parent()->upgrade(_cap, buf);
			});
	}
};

static Genode::Rm_session *rm_session() {
	using namespace Genode;
	static Expanding_rm_session_client rm (static_cap_cast<Rm_session>(env()->parent()->session("Env::rm_session", "")));
	return &rm;
}


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
		page = rm_session()->attach_at(ds_cap, phys);
	} catch (Rm_session::Out_of_metadata) {
		throw;
	} catch (Rm_session::Region_conflict) {
		/* memory already attached before - done */
		return;
	} catch (...) { }

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			rm_session()->detach(page);

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

void Platform::Device_pd_component::assign_pci(Genode::Io_mem_dataspace_capability io_mem_cap, Genode::uint16_t rid)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = rm_session()->attach(io_mem_cap);
	/* sanity check */
	if (!page)
		throw Rm_session::Region_conflict();

	/* trigger mapping of whole memory area */
	if (!map_eager(page, 12))
		PERR("assignment of PCI device failed - %lx", page);

	/* try to assign pci device to this protection domain */
	if (!env()->pd_session()->assign_pci(page, rid))
		PERR("assignment of PCI device %x:%x.%x failed phys=%lx virt=%lx",
		     rid >> 8, (rid >> 3) & 0x1f, rid & 0x7,
		     ds_client.phys_addr(), page);
	else
		PINF("assignment of %x:%x.%x succeeded",
		     rid >> 8, (rid >> 3) & 0x1f, rid & 0x7);

	/* we don't need the mapping anymore */
	rm_session()->detach(page);
}

int main(int argc, char **argv)
{
	using namespace Genode;

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

	Genode::sleep_forever();
	return 0;
}
