/*
 * \brief   Utilities for accessing information of the System Management BIOS
 * \author  Martin Stein
 * \date    2019-07-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SMBIOS__SMBIOS_H_
#define _SMBIOS__SMBIOS_H_

/* Genode includes */
#include <base/stdint.h>
#include <util/string.h>
#include <base/log.h>

namespace Genode::Smbios {

	inline uint8_t checksum(uint8_t const *base, size_t num_bytes)
	{
		uint8_t sum { 0 };
		for (uint8_t idx = 0; idx < num_bytes; idx++)
			sum = sum + base[idx];
		return sum;
	}

	struct Header;
	struct Dmi_entry_point;
	struct V2_entry_point;
	struct V3_entry_point;

	bool process_dmi(char const *anchor, addr_t ep_phy,
	                 auto &mem_fn, auto const &fn);
	bool process_v2(char const *anchor, addr_t ep_phy,
	                auto const &mem_fn, auto const &fn);
	bool process_v3(char const *anchor, addr_t ep_phy,
	                auto const &mem_fn, auto const &fn);

	bool from_scan(auto const &mem_fn, auto const &v3_fn,
	               auto const &v2_fn, auto const &dmi_fn);
	bool from_pointer(addr_t table,
	                  auto const &mem_fn, auto const &v3_fn,
	                  auto const &v2_fn, auto const &dmi_fn);
}


struct Genode::Smbios::Header
{
	enum Type {
		BIOS       = 0,
		SYSTEM     = 1,
		BASE_BOARD = 2,
	};

	uint8_t  type;
	uint8_t  length;
	uint16_t handle;
} __attribute__((packed));


struct Genode::Smbios::Dmi_entry_point
{
	enum { LENGTH = 15 };

	uint8_t  anchor_string[5];
	uint8_t  checksum;
	uint16_t struct_table_length;
	uint32_t struct_table_addr;
	uint16_t nr_of_structs;
	uint8_t  bcd_revision;

	bool valid() const { return Smbios::checksum(anchor_string, LENGTH) == 0; }
} __attribute__((packed));


struct Genode::Smbios::V2_entry_point
{
	enum { MAX_LENGTH    = 32 };
	enum { INTERM_LENGTH = 15 };

	uint8_t  anchor_string[4];
	uint8_t  checksum;
	uint8_t  length;
	uint8_t  version_major;
	uint8_t  version_minor;
	uint16_t max_struct_size;
	uint8_t  revision;
	uint8_t  formatted_area[5];
	uint8_t  interm_anchor_string[5];
	uint8_t  interm_checksum;
	uint16_t struct_table_length;
	uint32_t struct_table_addr;
	uint16_t nr_of_structs;
	uint8_t  bcd_revision;

	bool valid() const
	{
		return length <= MAX_LENGTH && Smbios::checksum(anchor_string, length) == 0;
	}

	bool interm_valid() const
	{
		return memcmp(interm_anchor_string, "_DMI_", 5) == 0
		    && Smbios::checksum(interm_anchor_string, INTERM_LENGTH) == 0;
	}

	Dmi_entry_point const &dmi_ep() const
	{
		return *(Dmi_entry_point const *)interm_anchor_string;
	}
} __attribute__((packed));


struct Genode::Smbios::V3_entry_point
{
	enum { MAX_LENGTH = 32 };

	uint8_t  anchor_string[5];
	uint8_t  checksum;
	uint8_t  length;
	uint8_t  version_major;
	uint8_t  version_minor;
	uint8_t  docrev;
	uint8_t  revision;
	uint8_t  reserved_0;
	uint32_t struct_table_max_size;
	uint64_t struct_table_addr;

	bool valid() const
	{
		return length <= MAX_LENGTH && Smbios::checksum(anchor_string, length) == 0;
	}
} __attribute__((packed));


bool Genode::Smbios::process_dmi(char   const *anchor,
                                 addr_t const  ep_phy,
                                 auto   const &mem_fn,
                                 auto   const &fn)
{
	if (memcmp(anchor, "_DMI_", 5))
		return false;

	return mem_fn(ep_phy, sizeof(Dmi_entry_point), [&] (Span const &m) {
		Dmi_entry_point const &ep = *(Dmi_entry_point const *)m.start;

		if (!ep.valid()) {
			warning("DMI entry point invalid");
			return false;
		}

		log("DMI table (entry point: ", (void *)anchor,
		    " structures: ", Hex(ep.struct_table_addr), ")");
		fn(ep);
		return true;
	});
}


bool Genode::Smbios::process_v2(char   const *anchor,
                                addr_t const  ep_phy,
                                auto   const &mem_fn,
                                auto   const &fn)
{
	if (memcmp(anchor, "_SM_",  4))
		return false;

	return mem_fn(ep_phy, sizeof(V2_entry_point), [&] (Span const &m) {
		V2_entry_point const &ep = *(V2_entry_point const *)m.start;

		if (!ep.valid()) {
			warning("SMBIOSv2 entry point invalid");
			return false;
		}
		if (!ep.interm_valid()) {
			warning("SMBIOSv2 entry point intermediate invalid");
			return false;
		}

		log("SMBIOSv2 table (entry point: ", (void *)anchor,
		    " structures: ", Hex(ep.struct_table_addr), ")");
		fn(ep);
		return true;
	});
}


bool Genode::Smbios::process_v3(char   const *anchor,
                                addr_t const  ep_phy,
                                auto   const &mem_fn,
                                auto   const &fn)
{
	if (memcmp(anchor, "_SM3_", 5))
		return false;

	return mem_fn(ep_phy, sizeof(V3_entry_point), [&] (Span const &m) {
		V3_entry_point const &ep = *(V3_entry_point const *)m.start;

		if (!ep.valid()) {
			warning("SMBIOSv3 entry point invalid");
			return false;
		}
		if (ep.struct_table_addr > (uint64_t)(~(addr_t)0)) {
			warning("SMBIOSv3 entry point bad structure-table address ",
			        (void *)ep.struct_table_addr);
			return false;
		}

		log("SMBIOSv3 table (entry point: ", (void *)anchor,
		    " structures: ", (void *)ep.struct_table_addr, ")");
		fn(ep);
		return true;
	});
}


bool Genode::Smbios::from_scan(auto const &mem_fn,
                               auto const &v3_fn,
                               auto const &v2_fn,
                               auto const &dmi_fn)
{
	enum { SCAN_BASE_PHY    = 0xf0000 };
	enum { SCAN_SIZE        = 0x10000 };
	enum { SCAN_SIZE_SMBIOS =  0xfff0 };
	enum { SCAN_STEP        =    0x10 };

	return mem_fn(SCAN_BASE_PHY, SCAN_SIZE, [&] (Span const &m) {
		for (unsigned i = 0; i < SCAN_SIZE_SMBIOS; i += SCAN_STEP )
			if (process_v3(m.start + i, SCAN_BASE_PHY + i, mem_fn, v3_fn))
				return true;
		for (unsigned i = 0; i < SCAN_SIZE_SMBIOS; i += SCAN_STEP )
			if (process_v2(m.start + i, SCAN_BASE_PHY + i, mem_fn, v2_fn))
				return true ;
		for (unsigned i = 0; i < SCAN_SIZE; i += SCAN_STEP )
			if (process_dmi(m.start + i, SCAN_BASE_PHY + i, mem_fn, dmi_fn))
				return true;
		return false;
	});
}


bool Genode::Smbios::from_pointer(addr_t const  table_phy,
                                  auto   const &mem_fn,
                                  auto   const &v3_fn,
                                  auto   const &v2_fn,
                                  auto   const &dmi_fn)
{
	return mem_fn(table_phy, 5, [&] (Span const &m) {
		return process_v3(m.start, table_phy, mem_fn, v3_fn)
		    || process_v2(m.start, table_phy, mem_fn, v2_fn)
		    || process_dmi(m.start, table_phy, mem_fn, dmi_fn);
	});
}

#endif /* _SMBIOS__SMBIOS_H_ */
