/*
 * \brief  Pci device protection for platform driver
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \date   2013-02-10
 */

/*
 * Copyright (C) 2013-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>

#include <util/retry.h>

#include <device_pd.h>

using namespace Driver;

Device_pd::Region_map_client::Local_addr
Device_pd::Region_map_client::attach(Dataspace_capability ds,
                                     size_t               size,
                                     off_t                offset,
                                     bool                 use_local_addr,
                                     Local_addr           local_addr,
                                     bool                 executable,
                                     bool                 writeable)
{
	return retry<Out_of_ram>(
		[&] () {
			return retry<Out_of_caps>(
				[&] () {
					return Genode::Region_map_client::attach(ds, size, offset,
					                                         use_local_addr,
					                                         local_addr,
					                                         executable,
					                                         writeable); },
				[&] () {
					upgrade_caps();
				}
			);
		},
		[&] () { upgrade_ram(); }
	);
}


void Device_pd::Region_map_client::upgrade_ram()
{
	Ram_quota const ram { 4096 };
	_ram_guard.withdraw(ram);
	_env.pd().transfer_quota(_pd.rpc_cap(), ram);
}


void Device_pd::Region_map_client::upgrade_caps()
{
	Cap_quota const caps { 2 };
	_cap_guard.withdraw(caps);
	_env.pd().transfer_quota(_pd.rpc_cap(), caps);
}


addr_t Device_pd::_dma_addr(addr_t const phys_addr,
                            size_t const size,
                            bool   const force_phys_addr)
{
	using Alloc_error = Allocator::Alloc_error;

	if (!_iommu) return phys_addr;

	/*
	 * 1:1 mapping (allocate at specified range from DMA memory allocator)
	 */
	if (force_phys_addr) {
		return _dma_alloc.alloc_addr(size, phys_addr).convert<addr_t>(
			[&] (void *) -> addr_t { return phys_addr; },
			[&] (Alloc_error err) -> addr_t {
				switch (err) {
				case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
				case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
				case Alloc_error::DENIED:
					error("Could not attach DMA range at ",
					      Hex_range(phys_addr, size), " (error: ", err, ")");
					break;
				}
				return 0UL;
			});
	}

	return _dma_alloc.alloc_aligned(size, 12).convert<addr_t>(
		[&] (void *ptr) { return (addr_t)ptr; },
		[&] (Alloc_error err) -> addr_t {
			switch (err) {
			case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
			case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
			case Alloc_error::DENIED:
				error("Could not allocate DMA area of size: ", size,
				      " total avail: ", _dma_alloc.avail(),
				     " (error: ", err, ")");
				break;
			};
			return 0;
		});
}


addr_t Device_pd::attach_dma_mem(Dataspace_capability ds_cap,
                                 addr_t const         phys_addr,
                                 bool   const         force_phys_addr)
{
	using namespace Genode;

	bool retry = false;

	Dataspace_client ds_client(ds_cap);
	size_t size     = ds_client.size();
	addr_t dma_addr = _dma_addr(phys_addr, size, force_phys_addr);

	if (dma_addr == 0) return 0;

	do {
		_pd.attach_dma(ds_cap, dma_addr).with_result(
			[&] (Pd_session::Attach_dma_ok) {
				/* trigger eager mapping of memory */
				_pd.map(dma_addr, ds_client.size());
				retry = false;
			},
			[&] (Pd_session::Attach_dma_error e) {
				switch (e) {
				case Pd_session::Attach_dma_error::OUT_OF_RAM:
					_address_space.upgrade_ram();
					retry = true;
					break;
				case Pd_session::Attach_dma_error::OUT_OF_CAPS:
					_address_space.upgrade_caps();
					retry = true;
					break;
				case Pd_session::Attach_dma_error::DENIED:
					_address_space.detach(dma_addr);
					error("Device PD: attach_dma denied!");
					break;
				}
			}
		);
	} while (retry);

	return dma_addr;
}


void Device_pd::free_dma_mem(addr_t dma_addr)
{
	if (_iommu)
		_dma_alloc.free((void *)dma_addr);
}


void Device_pd::assign_pci(Io_mem_dataspace_capability const io_mem_cap,
                           Pci::Bdf                    const bdf)
{
	addr_t addr = _address_space.attach(io_mem_cap, 0x1000);

	/* sanity check */
	if (!addr)
		throw Region_map::Region_conflict();

	/* trigger eager mapping of memory */
	_pd.map(addr, 0x1000);

	/* try to assign pci device to this protection domain */
	if (!_pd.assign_pci(addr, Pci::Bdf::rid(bdf)))
		log("Assignment of PCI device ", bdf, " to device PD failed, no IOMMU?!");

	/* we don't need the mapping anymore */
	_address_space.detach(addr);
}


Device_pd::Device_pd(Env             & env,
                     Allocator       & md_alloc,
                     Ram_quota_guard & ram_guard,
                     Cap_quota_guard & cap_guard,
                     bool const iommu)
:
	_pd(env, Pd_connection::Device_pd()),
	_dma_alloc(&md_alloc), _iommu(iommu),
	_address_space(env, _pd, ram_guard, cap_guard)
{
	/* 0x1000 - 4GB per device PD */
	enum { DMA_SIZE = 0xffffe000 };
	_dma_alloc.add_range(0x1000, DMA_SIZE);

	_pd.ref_account(env.pd_session_cap());
}
