/*
 * \brief  Pci device protection domain service for platform driver 
 * \author Alexander Boettcher
 * \date   2013-02-10
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <os/static_root.h>
#include <base/log.h>
#include <base/sleep.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>

#include <util/flex_iterator.h>
#include <util/retry.h>

#include <nova/native_thread.h>

#include "../pci_device_pd_ipc.h"


/**
 * Custom handling of PD-session depletion during attach operations
 *
 * The default implementation of 'env.rm()' automatically issues a resource
 * request if the PD session quota gets exhausted. For the device PD, we don't
 * want to issue resource requests but let the platform driver reflect this
 * condition to its client.
 */
struct Expanding_region_map_client : Genode::Region_map_client
{
	Genode::Env &_env;

	Expanding_region_map_client(Genode::Env &env)
	:
		Region_map_client(env.pd().address_space()), _env(env)
	{ }

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

				if (_env.ram().avail() < UPGRADE_QUOTA)
					throw;

				Genode::String<32> arg("ram_quota=", (unsigned)UPGRADE_QUOTA);
				_env.upgrade(Genode::Parent::Env::pd(), arg.string());
			}
		);
	}
};


static bool map_eager(Genode::addr_t const page, unsigned log2_order)
{
	using Genode::addr_t;

	Genode::Thread * myself = Genode::Thread::myself();
	Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(myself->utcb());
	Nova::Rights const mapping_rw(true, true, false);

	addr_t const page_fault_portal = myself->native_thread().exc_pt_sel + 14;

	while (true) {
		/* setup faked page fault information */
		utcb->set_msg_word(((addr_t)&utcb->qual[2] - (addr_t)utcb->msg) /
		                   sizeof(addr_t));
		utcb->ip      = reinterpret_cast<addr_t>(map_eager);
		utcb->qual[1] = page;
		utcb->crd_rcv = Nova::Mem_crd(page >> 12, log2_order - 12, mapping_rw);

		/* trigger faked page fault */
		Genode::uint8_t res = Nova::call(page_fault_portal);

		bool const retry = utcb->msg_words();
		if (res != Nova::NOVA_OK || !retry)
			return res == Nova::NOVA_OK;
	};
}


void Platform::Device_pd_component::attach_dma_mem(Genode::Dataspace_capability ds_cap)
{
	using namespace Genode;

	Dataspace_client ds_client(ds_cap);

	addr_t const phys = ds_client.phys_addr();
	size_t const size = ds_client.size();

	addr_t page = ~0UL;

	try {
		page = _address_space.attach_at(ds_cap, phys);
	} catch (Rm_session::Out_of_metadata) {
		throw;
	} catch (Rm_session::Region_conflict) {
		/*
		 * DMA memory already attached before or collision with normal
		 * device_pd memory (text, data, etc).
		 * Currently we can't distinguish it easily - show error
		 * message as a precaution.
		 */
		Genode::error("region conflict");
	} catch (...) { }

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			_address_space.detach(page);

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

void Platform::Device_pd_component::assign_pci(Genode::Io_mem_dataspace_capability io_mem_cap,
                                               Genode::uint16_t rid)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = _address_space.attach(io_mem_cap);
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
			      Hex((v >> 3) & 0x1f, Hex::Prefix::OMIT_PREFIX), ".",
			      Hex(v & 0x7, Hex::Prefix::OMIT_PREFIX));
		}
	};

	/* try to assign pci device to this protection domain */
	if (!_env.pd().assign_pci(page, rid))
		Genode::error("assignment of PCI device ", Rid(rid), " failed ",
		              "phys=", Genode::Hex(ds_client.phys_addr()), " "
		              "virt=", Genode::Hex(page));
	else
		Genode::log("assignment of PCI device ", Rid(rid), " succeeded");

	/* we don't need the mapping anymore */
	_address_space.detach(page);
}


struct Main
{
	Genode::Env &env;

	Expanding_region_map_client rm { env };

	Platform::Device_pd_component pd_component { rm, env };

	Genode::Static_root<Platform::Device_pd> root { env.ep().manage(pd_component) };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) { static Main main(env); }

