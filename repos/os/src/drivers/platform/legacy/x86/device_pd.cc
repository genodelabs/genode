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

void Platform::Device_pd::attach_dma_mem(Dataspace_capability ds_cap,
                                         addr_t const dma_addr)
{
	using namespace Genode;

	Dataspace_client ds_client(ds_cap);

	size_t const size = ds_client.size();

	addr_t page = ~0UL;

	using Attach_dma_error = Pd_session::Attach_dma_error;

	bool retry = false;

	do {
		_pd.attach_dma(ds_cap, dma_addr).with_result(

			[&] (Pd_session::Attach_dma_ok) {

				page = dma_addr;

				/* trigger eager mapping of memory */
				_pd.map(page, size);
				retry = false;
			},
			[&] (Attach_dma_error e) {
				switch (e) {
				case Attach_dma_error::OUT_OF_RAM:
					_address_space.upgrade_ram();
					retry = true;
					break;
				case Attach_dma_error::OUT_OF_CAPS:
					_address_space.upgrade_caps();
					retry = true;
					break;
				case Attach_dma_error::DENIED:
					warning("Pd_session::attach_dma denied");
					page = dma_addr;
					retry = false;
					break;
				}
			}
		);
	} while (retry);

	/* sanity check */
	if ((page == ~0UL) || (page != dma_addr)) {
		if (page != ~0UL)
			_address_space.detach(page);

		error(_label, ": attachment of DMA memory @ ",
		      Hex(dma_addr), "+", Hex(size), " " "failed page=", Hex(page));
		return;
	}
}

void Platform::Device_pd::assign_pci(Io_mem_dataspace_capability const io_mem_cap,
                                     addr_t const offset, uint16_t const rid)
{
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
		uint16_t const v;
		explicit Rid(uint16_t rid) : v(rid) { }
		void print(Output &out) const
		{
			using Genode::print;
			print(out, Hex((uint8_t)(v >> 8), Hex::Prefix::OMIT_PREFIX, Hex::PAD), ":",
			      Hex((uint8_t)((v >> 3) & 0x1f), Hex::Prefix::OMIT_PREFIX, Hex::PAD), ".",
			      Hex(v & 0x7, Hex::Prefix::OMIT_PREFIX));
		}
	};

	/* try to assign pci device to this protection domain */
	if (!_pd.assign_pci(page, rid))
		error(_label, ": assignment of PCI device ", Rid(rid), " failed ",
		      "virt=", Hex(page));
	else
		log(_label,": assignment of PCI device ", Rid(rid), " succeeded");

	/* we don't need the mapping anymore */
	_address_space.detach(page);
}
