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
#include <base/log.h>
#include <base/sleep.h>

#include <os/server.h>

#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>

#include <util/flex_iterator.h>
#include <util/retry.h>

#include <nova/native_thread.h>

#include "../pci_device_pd_ipc.h"


struct Expanding_region_map_client : Genode::Region_map_client
{
	Expanding_region_map_client(Genode::Capability<Region_map> cap)
	: Region_map_client(cap) { }

	Local_addr attach(Genode::Dataspace_capability ds,
	                          Genode::size_t size, Genode::off_t offset,
	                          bool use_local_addr,
	                          Local_addr local_addr,
	                          bool executable) override
	{
		return Genode::retry<Genode::Region_map::Out_of_metadata>(
			[&] () {
				return Region_map_client::attach(ds, size, offset,
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

				Genode::env()->parent()->upgrade(Genode::env()->pd_session_cap(), buf);
			});
	}
};


static Genode::Region_map &address_space() {
	using namespace Genode;
	static Expanding_region_map_client rm(Genode::env()->pd_session()->address_space());
	return rm;
}


static bool map_eager(Genode::addr_t const page, unsigned log2_order)
{
	using Genode::addr_t;

	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());
	Nova::Rights const mapping_rw(true, true, false);

	addr_t const page_fault_portal = myself->native_thread().exc_pt_sel + 14;

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
		page = address_space().attach_at(ds_cap, phys);
	} catch (Rm_session::Out_of_metadata) {
		throw;
	} catch (Rm_session::Region_conflict) {
		/* memory already attached before - done */
		return;
	} catch (...) { }

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			address_space().detach(page);

		Genode::error("attachment of DMA memory @ ",
		              Genode::Hex(phys), "+", Genode::Hex(size), " failed");
		return;
	}

	Genode::Flexpage_iterator it(page, size, page, size, 0);
	for (Genode::Flexpage flex = it.page(); flex.valid(); flex = it.page()) {
		if (map_eager(flex.addr, flex.log2_order))
			continue;

		Genode::error("attachment of DMA memory @ ",
		              Genode::Hex(phys), "+", Genode::Hex(size), " failed at ",
		              flex.addr);
		return;
	}
}

void Platform::Device_pd_component::assign_pci(Genode::Io_mem_dataspace_capability io_mem_cap, Genode::uint16_t rid)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = address_space().attach(io_mem_cap);
	/* sanity check */
	if (!page)
		throw Rm_session::Region_conflict();

	/* trigger mapping of whole memory area */
	if (!map_eager(page, 12))
		Genode::error("assignment of PCI device failed - ", Genode::Hex(page));

	/* utility to print rid value */
	struct Rid
	{
		Genode::uint16_t const v;
		explicit Rid(Genode::uint16_t rid) : v(rid) { }
		void print(Genode::Output &out) const
		{
			using Genode::print;
			using Genode::Hex;
			print(out, Hex(v >> 8, Hex::Prefix::OMIT_PREFIX), ":",
			      Hex((v >> 3) & 3, Hex::Prefix::OMIT_PREFIX), ".",
			      Hex(v & 0x7, Hex::Prefix::OMIT_PREFIX));
		}
	};

	/* try to assign pci device to this protection domain */
	if (!env()->pd_session()->assign_pci(page, rid))
		Genode::error("assignment of PCI device ", Rid(rid), " failed ",
		              "phys=", Genode::Hex(ds_client.phys_addr()), " "
		              "virt=", Genode::Hex(page));
	else
		Genode::log("assignment of ", rid, " succeeded");

	/* we don't need the mapping anymore */
	address_space().detach(page);
}


using namespace Genode;


struct Main
{
	Server::Entrypoint &ep;

	Platform::Device_pd_component pd_component;
	Static_root<Platform::Device_pd> root;

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep.manage(pd_component))
	{
		env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "device_pd_ep";    }
	size_t stack_size()            { return 1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);   }
}
