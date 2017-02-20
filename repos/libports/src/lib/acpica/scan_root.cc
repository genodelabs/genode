/*
 * \brief  Lookup code for initial ACPI RSDP pointer
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <io_mem_session/connection.h>
#include <base/attached_io_mem_dataspace.h>

#include "env.h"

extern "C" {
#include "acpi.h"
}

namespace Genode {
	class Acpi_table;
}

class Genode::Acpi_table
{
	private:

		/* BIOS range to scan for RSDP */
		enum { BIOS_BASE = 0xe0000, BIOS_SIZE = 0x20000 };

		/**
		 * Search for RSDP pointer signature in area
		 */
		uint8_t *_search_rsdp(uint8_t *area)
		{
			for (addr_t addr = 0; area && addr < BIOS_SIZE; addr += 16)
 				/* XXX checksum table */
				if (!memcmp(area + addr, "RSD PTR ", 8))
					return area + addr;

			return nullptr;
		}

		/**
		 * Return 'Root System Descriptor Pointer' (ACPI spec 5.2.5.1)
		 */
		uint64_t _rsdp()
		{
			uint8_t * local = 0;

			Genode::Env &env = Acpica::env();

			/* try BIOS area */
			{
				Genode::Attached_io_mem_dataspace io_mem(env, BIOS_BASE, BIOS_SIZE);
				local = _search_rsdp(io_mem.local_addr<uint8_t>());
				if (local)
					return BIOS_BASE + (local - io_mem.local_addr<uint8_t>());
			}

			/* search EBDA (BIOS addr + 0x40e) */
			try {
				unsigned short base = 0;
				{
					Genode::Attached_io_mem_dataspace io_mem(env, 0, 0x1000);
					local = io_mem.local_addr<uint8_t>();
					if (local)
						base = (*reinterpret_cast<unsigned short *>(local + 0x40e)) << 4;
				}

				if (!base)
					return 0;

				Genode::Attached_io_mem_dataspace io_mem(env, base, 1024);
				local = _search_rsdp(io_mem.local_addr<uint8_t>());

				if (local)
					return base + (local - io_mem.local_addr<uint8_t>());
			} catch (...) {
				Genode::warning("failed to scan EBDA for RSDP root");
			}

			return 0;
		}

	public:

		Acpi_table() { }

		Genode::addr_t phys_rsdp()
		{
			Genode::uint64_t phys_rsdp = _rsdp();
			return phys_rsdp;
		}
};

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer (void)
{
	Genode::Acpi_table acpi_table;
	ACPI_PHYSICAL_ADDRESS rsdp = acpi_table.phys_rsdp();
	return rsdp;
}
