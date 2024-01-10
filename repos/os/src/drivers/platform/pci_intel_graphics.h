/*
 * \brief  Platform driver - PCI intel graphics utilities
 * \author Stefan Kalkowski
 * \date   2022-06-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/mmio.h>
#include <pci/config.h>
#include <device.h>

namespace Driver {
	void pci_intel_graphics_info(Device::Pci_config const & cfg,
	                             Env                      & env,
	                             Device_model             & model,
	                             Xml_generator            & xml);
}


static inline unsigned pci_intel_graphics_generation(Pci::device_t id)
{
	struct Device_gen {
		Pci::device_t id;
		unsigned      gen;
	};

	static Device_gen intel_gpu_generations[] = {
		{ 0x7121, 1 }, { 0x7123, 1 }, { 0x7125, 1 }, { 0x1132, 1 },
		{ 0x3577, 2 }, { 0x2562, 2 }, { 0x3582, 2 }, { 0x358e, 2 },
		{ 0x2572, 2 }, { 0x2582, 3 }, { 0x258a, 3 }, { 0x2592, 3 },
		{ 0x2772, 3 }, { 0x27a2, 3 }, { 0x27ae, 3 }, { 0x29b2, 3 },
		{ 0x29c2, 3 }, { 0x29d2, 3 }, { 0xa001, 3 }, { 0xa011, 3 },
		{ 0x2972, 4 }, { 0x2982, 4 }, { 0x2992, 4 }, { 0x29a2, 4 },
		{ 0x2a02, 4 }, { 0x2a12, 4 }, { 0x2a42, 4 }, { 0x2e02, 4 },
		{ 0x2e12, 4 }, { 0x2e22, 4 }, { 0x2e32, 4 }, { 0x2e42, 4 },
		{ 0x2e92, 4 }, { 0x0042, 5 }, { 0x0046, 5 }, { 0x0102, 6 },
		{ 0x010a, 6 }, { 0x0112, 6 }, { 0x0122, 6 }, { 0x0106, 6 },
		{ 0x0116, 6 }, { 0x0126, 6 }, { 0x0156, 6 }, { 0x0166, 6 },
		{ 0x0152, 7 }, { 0x015a, 7 }, { 0x0162, 7 }, { 0x016a, 7 },
		{ 0x0a02, 7 }, { 0x0a06, 7 }, { 0x0a0a, 7 }, { 0x0a0b, 7 },
		{ 0x0a0e, 7 }, { 0x0402, 7 }, { 0x0406, 7 }, { 0x040a, 7 },
		{ 0x040b, 7 }, { 0x040e, 7 }, { 0x0c02, 7 }, { 0x0c06, 7 },
		{ 0x0c0a, 7 }, { 0x0c0b, 7 }, { 0x0c0e, 7 }, { 0x0d02, 7 },
		{ 0x0d06, 7 }, { 0x0d0a, 7 }, { 0x0d0b, 7 }, { 0x0d0e, 7 },
		{ 0x0a12, 7 }, { 0x0a16, 7 }, { 0x0a1a, 7 }, { 0x0a1b, 7 },
		{ 0x0a1e, 7 }, { 0x0412, 7 }, { 0x0416, 7 }, { 0x041a, 7 },
		{ 0x041b, 7 }, { 0x041e, 7 }, { 0x0c12, 7 }, { 0x0c16, 7 },
		{ 0x0c1a, 7 }, { 0x0c1b, 7 }, { 0x0c1e, 7 }, { 0x0d12, 7 },
		{ 0x0d16, 7 }, { 0x0d1a, 7 }, { 0x0d1b, 7 }, { 0x0d1e, 7 },
		{ 0x0a22, 7 }, { 0x0a26, 7 }, { 0x0a2a, 7 }, { 0x0a2b, 7 },
		{ 0x0a2e, 7 }, { 0x0422, 7 }, { 0x0426, 7 }, { 0x042a, 7 },
		{ 0x042b, 7 }, { 0x042e, 7 }, { 0x0c22, 7 }, { 0x0c26, 7 },
		{ 0x0c2a, 7 }, { 0x0c2b, 7 }, { 0x0c2e, 7 }, { 0x0d22, 7 },
		{ 0x0d26, 7 }, { 0x0d2a, 7 }, { 0x0d2b, 7 }, { 0x0d2e, 7 },
		{ 0x0f30, 7 }, { 0x0f31, 7 }, { 0x0f32, 7 }, { 0x0f33, 7 },
	};
	for (unsigned i = 0;
	     i < (sizeof(intel_gpu_generations)/(sizeof(Device_gen)));
	     i++) {
		if (id == intel_gpu_generations[i].id)
			return intel_gpu_generations[i].gen;
	};

	/*
	 * If we do not find something in the array, we assume its
	 * generation 8 or higher
	 */
	return 8;
};


void Driver::pci_intel_graphics_info(Device::Pci_config const & cfg,
                                     Env                      & env,
                                     Device_model             & model,
                                     Xml_generator            & xml)
{
	enum {
		GPU_CLASS_MASK = 0xff0000,
		GPU_CLASS_ID   = 0x030000,
		VENDOR_INTEL   = 0x8086
	};

	if (((cfg.class_code & GPU_CLASS_MASK) != GPU_CLASS_ID) ||
	    (cfg.vendor_id != VENDOR_INTEL))
		return;

	/* PCI configuration registers of host bridge */
	struct Host_bridge : Mmio<0x54>
	{
		struct Gen_old_gmch_control : Register<0x52, 16> {};
		struct Gen_gmch_control     : Register<0x50, 16> {};

		using Mmio::Mmio;
	};

	/* find host bridge */
	model.for_each([&] (Device const & dev) {
		dev.for_pci_config([&] (Device::Pci_config const &cfg) {
			if (cfg.bus_num || cfg.dev_num || cfg.func_num)
				return;

			static constexpr size_t IO_MEM_SIZE = 0x1000;

			Attached_io_mem_dataspace io_mem(env, cfg.addr, IO_MEM_SIZE);
			Host_bridge               config({io_mem.local_addr<char>(), IO_MEM_SIZE});
			unsigned gen  = pci_intel_graphics_generation(cfg.device_id);
			uint16_t gmch = 0;

			if (gen < 6)
				gmch = config.read<Host_bridge::Gen_old_gmch_control>();
			else
				gmch = config.read<Host_bridge::Gen_gmch_control>();

			xml.attribute("intel_gmch_control", String<16>(Hex(gmch)));
		});
	});
}
