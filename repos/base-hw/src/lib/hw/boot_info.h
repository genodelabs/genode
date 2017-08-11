/*
 * \brief   Boot information
 * \author  Stefan Kalkowski
 * \date    2016-10-26
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__BOOT_INFO_H_
#define _SRC__LIB__HW__BOOT_INFO_H_

#include <hw/mmio_space.h>
#include <hw/acpi_rsdp.h>

namespace Hw { struct Boot_info; }

struct Hw::Boot_info
{
	using Mapping_pool = Array<Mapping, 32>;

	addr_t        const table;
	addr_t        const table_allocator;
	Mapping_pool  const elf_mappings;
	Mmio_space    const mmio_space;
	Memory_region_array ram_regions;
	Acpi_rsdp     const acpi_rsdp;

	Boot_info(addr_t       const table,
	          addr_t       const table_alloc,
	          Mapping_pool const elf_mappings,
	          Mmio_space   const mmio_space,
	          Acpi_rsdp    const &acpi_rsdp)
	: table(table), table_allocator(table_alloc),
	  elf_mappings(elf_mappings), mmio_space(mmio_space),
	  acpi_rsdp(acpi_rsdp) {}
};

#endif /* _SRC__LIB__HW__BOOT_INFO_H_ */
