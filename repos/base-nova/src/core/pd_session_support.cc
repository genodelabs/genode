/*
 * \brief  Extension of core implementation of the PD session interface
 * \author Alexander Boettcher
 * \date   2013-01-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core */
#include <pd_session_component.h>
#include <assertion.h>

using namespace Genode;


bool Pd_session_component::assign_pci(addr_t pci_config_memory, uint16_t bdf)
{
	uint8_t res = Nova::NOVA_PD_OOM;
	do {
		res = Nova::assign_pci(_pd->pd_sel(), pci_config_memory, bdf);
	} while (res == Nova::NOVA_PD_OOM &&
	         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
	                                                   _pd->pd_sel(),
	                                                   "core", "ep",
	                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

	return res == Nova::NOVA_OK;
}


void Pd_session_component::map(addr_t virt, addr_t size)
{
	Genode::addr_t const  pd_core   = platform_specific().core_pd_sel();
	Platform_pd          &target_pd = *_pd;
	Genode::addr_t const  pd_dst    = target_pd.pd_sel();
	Nova::Utcb           &utcb      = *reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());

	auto lambda = [&] (Region_map_component *region_map,
	                   Rm_region            *region,
	                   addr_t const          ds_offset,
	                   addr_t const          region_offset,
	                   addr_t const          dst_region_size) -> addr_t
	{
		Dataspace_component * dsc = region ? &region->dataspace() : nullptr;
		if (!dsc) {
			struct No_dataspace{};
			throw No_dataspace();
		}
		if (!region_map) {
			ASSERT_NEVER_CALLED;
		}

		Mapping mapping = Region_map_component::create_map_item(region_map,
		                                                        *region,
		                                                        ds_offset,
		                                                        region_offset,
		                                                        *dsc, virt,
		                                                        dst_region_size);

		/* asynchronously map memory */
		uint8_t err = Nova::NOVA_PD_OOM;
		do {
			utcb.set_msg_word(0);
			bool res = utcb.append_item(mapping.mem_crd(), 0, true, false,
			                            false, mapping.dma(),
			                            mapping.write_combined());
			/* one item ever fits on the UTCB */
			(void)res;

			Nova::Rights const map_rights (true,
			                               region->write() && dsc->writable(),
			                              region->executable());

			/* receive window in destination pd */
			Nova::Mem_crd crd_mem(mapping.dst_addr() >> 12,
			                      mapping.mem_crd().order(), map_rights);

			err = Nova::delegate(pd_core, pd_dst, crd_mem);
		} while (err == Nova::NOVA_PD_OOM &&
		         Nova::NOVA_OK == Pager_object::handle_oom(Pager_object::SRC_CORE_PD,
		                                                   _pd->pd_sel(),
		                                                   "core", "ep",
		                                                   Pager_object::Policy::UPGRADE_CORE_TO_DST));

		addr_t const map_crd_size = 1UL << (mapping.mem_crd().order() + 12);
		addr_t const mapped   = mapping.dst_addr() + map_crd_size - virt;

		if (err != Nova::NOVA_OK)
			error("could not map memory ",
			      Hex_range<addr_t>(mapping.dst_addr(), map_crd_size) , " "
			      "eagerly error=", err);

		return mapped;
	};

	try {
		while (size) {
			addr_t mapped = _address_space.apply_to_dataspace(virt, lambda);
			virt         += mapped;
			size          = size < mapped ? size : size - mapped;
		}
	} catch (...) {
		error(__func__, " failed ", Hex(virt), "+", Hex(size));
	}
}
