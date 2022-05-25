/*
 * \brief  Lookup Intel opregion region and report it as is (plain data)
 * \author Alexander Boettcher
 * \date   2022-05-25
 */

 /*
  * Copyright (C) 2022 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU Affero General Public License version 3.
  */

#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <os/reporter.h>

#include "intel_opregion.h"

void Acpi::Intel_opregion::generate_report(Genode::Env &env,
                                           addr_t const region_phys,
                                           addr_t const region_size)
{
	try {
		addr_t const phys_addr_offset = region_phys & 0xffful;
		addr_t const memory_size      = region_size + phys_addr_offset;

		/* create ram dataspace with space for io_mem address + size */
		Attached_io_mem_dataspace  io_mem { env, region_phys, memory_size };
		Attached_ram_dataspace report_mem { env.ram(), env.rm(),
		                                    memory_size + sizeof(addr_t) * 2 };

		auto mem_local = report_mem.local_addr<char>();

		/* copy io_mem to ram dataspace and preserve offset */
		memcpy(mem_local + phys_addr_offset, io_mem.local_addr<char>(),
		       region_size);

		Dataspace_client report_ds(report_mem.cap());

		/* report also io_mem address and io_mem size (!equal to ds size) */
		auto report_phys_ptr  = (addr_t*)(mem_local + report_ds.size() - sizeof(addr_t) * 2);
		auto report_phys_size = (addr_t*)(mem_local + report_ds.size() - sizeof(addr_t));

		*report_phys_ptr  = region_phys;
		*report_phys_size = region_size;

		/* create report */
		_report.construct(env, "intel_opregion", "intel_opregion",
		                  report_ds.size());
		_report->enabled(true);
		_report->report(report_mem.local_addr<void>(), report_ds.size());

	} catch (...) {
		Genode::warning("Intel opregion region copy failed");
	}
}
