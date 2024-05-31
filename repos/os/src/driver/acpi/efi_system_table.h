/*
 * \brief   Utilities for accessing an EFI system table
 * \author  Martin Stein
 * \date    2019-07-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EFI_SYSTEM_TABLE_H_
#define _EFI_SYSTEM_TABLE_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { struct Efi_system_table; }

struct Genode::Efi_system_table
{
	struct Header
	{
		uint64_t signature;
		uint32_t revision;
		uint32_t header_size;
		uint32_t crc32;
		uint32_t reserved;

	} __attribute__((packed));

	struct Guid
	{
		uint32_t data_1;
		uint16_t data_2;
		uint16_t data_3;
		uint8_t  data_4[8];

	} __attribute__((packed));

	struct Configuration_table
	{
		Guid     vendor_guid;
		uint64_t vendor_table;

	} __attribute__((packed));

	Header   header;
	uint64_t firmware_vendor;
	uint32_t firmware_revision;
	uint32_t reserved_0;
	uint64_t console_in_handle;
	uint64_t console_in;
	uint64_t console_out_handle;
	uint64_t console_out;
	uint64_t standard_error_handle;
	uint64_t standard_error;
	uint64_t runtime_services;
	uint64_t boot_services;
	uint64_t nr_of_table_entries;
	uint64_t config_table;

	template <typename PHY_MEM_FN,
	          typename HANDLE_TABLE_FN>
	void for_smbios_table(PHY_MEM_FN      const &phy_mem,
	                      HANDLE_TABLE_FN const &handle_table) const
	{
		Guid const guid { 0xeb9d2d31, 0x2d88, 0x11d3,
		                  { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } };

		Configuration_table const *cfg_table { (Configuration_table *)
			phy_mem(config_table, nr_of_table_entries *
			        sizeof(Configuration_table)) };

		for (unsigned long idx = 0; idx < nr_of_table_entries; idx++) {
			if (memcmp(&guid, &cfg_table[idx].vendor_guid, sizeof(Guid)) == 0) {
				handle_table((addr_t)cfg_table[idx].vendor_table);
			}
		}
	}

} __attribute__((packed));

#endif /* _EFI_SYSTEM_TABLE_H_ */
