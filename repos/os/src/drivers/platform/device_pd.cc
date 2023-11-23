/*
 * \brief  Pci device protection for platform driver
 * \author Alexander Boettcher
 * \author Stefan Kalkowski
 * \author Johannes Schlatow
 * \date   2013-02-10
 */

/*
 * Copyright (C) 2013-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <dataspace/client.h>
#include <region_map/client.h>
#include <pd_session/client.h>
#include <util/retry.h>

/* local includes */
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


void Device_pd::add_range(Io_mmu::Range        const & range,
                          addr_t               const,
                          Dataspace_capability const   cap)
{
	using namespace Genode;

	bool retry = false;

	if (range.start == 0) return;

	do {
		_pd.attach_dma(cap, range.start).with_result(
			[&] (Pd_session::Attach_dma_ok) {
				/* trigger eager mapping of memory */
				_pd.map(range.start, range.size);
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
					_address_space.detach(range.start);
					error("Device PD: attach_dma denied!");
					break;
				}
			}
		);
	} while (retry);
}


void Device_pd::remove_range(Io_mmu::Range const & range)
{
	_address_space.detach(range.start);
}


void Device_pd::enable_pci_device(Io_mem_dataspace_capability const io_mem_cap,
                                  Pci::Bdf                    const & bdf)
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


void Device_pd::disable_pci_device(Pci::Bdf const &)
{
	warning("Cannot unassign PCI device from device PD (not implemented by kernel).");
}



Device_pd::Device_pd(Env                        & env,
                     Ram_quota_guard            & ram_guard,
                     Cap_quota_guard            & cap_guard,
                     Kernel_iommu               & io_mmu,
                     Allocator                  & md_alloc,
                     Registry<Dma_buffer> const & buffer_registry)

:
	Io_mmu::Domain(io_mmu, md_alloc),
	_pd(env, Pd_connection::Device_pd()),
	_address_space(env, _pd, ram_guard, cap_guard)
{
	_pd.ref_account(env.pd_session_cap());

	buffer_registry.for_each([&] (Dma_buffer const & buf) {
		add_range({ buf.dma_addr, buf.size }, buf.phys_addr, buf.cap); });
}
