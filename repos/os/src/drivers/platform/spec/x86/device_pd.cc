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

#include <base/log.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>

#include <util/retry.h>

#include "device_pd.h"

void Platform::Device_pd::attach_dma_mem(Genode::Dataspace_capability ds_cap)
{
	using namespace Genode;

	Dataspace_client ds_client(ds_cap);

	addr_t const phys = ds_client.phys_addr();
	size_t const size = ds_client.size();

	addr_t page = ~0UL;

	try {
		page = _address_space.attach_at(ds_cap, phys);
		/* trigger eager mapping of memory */
		_pd.map(page, size);
	}
	catch (Out_of_ram)  { throw; }
	catch (Out_of_caps) { throw; }
	catch (Region_map::Region_conflict) {
		/*
		 * DMA memory already attached before.
		 */
		page = phys;
	} catch (...) {
		error(_label, ": attach_at or map failed");
	}

	/* sanity check */
	if ((page == ~0UL) || (page != phys)) {
		if (page != ~0UL)
			_address_space.detach(page);

		Genode::error(_label, ": attachment of DMA memory @ ",
		              Genode::Hex(phys), "+", Genode::Hex(size), " failed page=", Genode::Hex(page));
		return;
	}
}

void Platform::Device_pd::assign_pci(Genode::Io_mem_dataspace_capability const io_mem_cap,
                                     Genode::addr_t const   offset,
                                     Genode::uint16_t const rid)
{
	using namespace Genode;

	Dataspace_client ds_client(io_mem_cap);

	addr_t page = _address_space.attach(io_mem_cap, 0x1000, offset);

	/* sanity check */
	if (!page)
		throw Region_map::Region_conflict();

	/* trigger eager mapping of memory */
	_pd.map(page, 0x1000);

	/* utility to print rid value */
	struct Rid
	{
		Genode::uint16_t const v;
		explicit Rid(Genode::uint16_t rid) : v(rid) { }
		void print(Genode::Output &out) const
		{
			using Genode::print;
			using Genode::Hex;
			print(out, Hex((uint8_t)(v >> 8), Hex::Prefix::OMIT_PREFIX, Hex::PAD), ":",
			      Hex((uint8_t)((v >> 3) & 0x1f), Hex::Prefix::OMIT_PREFIX, Hex::PAD), ".",
			      Hex(v & 0x7, Hex::Prefix::OMIT_PREFIX));
		}
	};

	/* try to assign pci device to this protection domain */
	if (!_pd.assign_pci(page, rid))
		Genode::error(_label, ": assignment of PCI device ", Rid(rid), " failed ",
		              "phys=", Genode::Hex(ds_client.phys_addr() + offset), " "
		              "virt=", Genode::Hex(page));
	else
		Genode::log(_label,": assignment of PCI device ", Rid(rid), " succeeded");

	/* we don't need the mapping anymore */
	_address_space.detach(page);
}
