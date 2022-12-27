/*
 * \brief  Linux emulation backend functions
 * \author Josef Soentgen
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* lx emulation includes */
#include <lx_kit/env.h>

/* local includes */
#include "lx_emul.h"


void *emul_alloc_shmem_file_buffer(unsigned long size)
{
	if (!size)
		return nullptr;

	auto &buffer = Lx_kit::env().memory.alloc_buffer(size);
	return reinterpret_cast<void *>(buffer.virt_addr());
}


void emul_free_shmem_file_buffer(void *addr)
{
	Lx_kit::env().memory.free_buffer(addr);
}


unsigned short emul_intel_gmch_control_reg()
{
	using namespace Genode;

	unsigned short ret = 0;
	Lx_kit::env().devices.with_xml([&] (Xml_node node) {
		node.for_each_sub_node("device", [&] (Xml_node node) {
			node.for_each_sub_node("pci-config", [&] (Xml_node node) {
				unsigned short gmch =
					node.attribute_value<unsigned short>("intel_gmch_control", 0U);
				if (gmch) ret = gmch;
			});
		});
	});

	return ret;
}


unsigned long long emul_avail_ram()
{
	return Lx_kit::env().env.pd().avail_ram().value;
}
