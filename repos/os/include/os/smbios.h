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

#ifndef _OS__SMBIOS_H_
#define _OS__SMBIOS_H_

/* Genode includes */
#include <base/stdint.h>
#include <util/string.h>
#include <base/log.h>

namespace Genode {

	namespace Smbios_table { };

	inline bool smbios_checksum_correct(uint8_t const *base, uint8_t size)
	{
		uint8_t sum { 0 };
		for (uint8_t idx = 0; idx < size; idx++) {
			sum += base[idx];
		}
		return sum == 0;
	}

	struct Smbios_3_entry_point;
	struct Smbios_entry_point;
	struct Dmi_entry_point;
	struct Smbios_structure;
}


struct Genode::Smbios_structure
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


struct Genode::Dmi_entry_point
{
	enum { LENGTH = 15 };

	uint8_t  anchor_string[5];
	uint8_t  checksum;
	uint16_t struct_table_length;
	uint32_t struct_table_addr;
	uint16_t nr_of_structs;
	uint8_t  bcd_revision;

	bool checksum_correct() const
	{
		return smbios_checksum_correct(anchor_string, LENGTH);
	}

} __attribute__((packed));


struct Genode::Smbios_3_entry_point
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

	bool length_valid() const
	{
		return length <= MAX_LENGTH;
	}

	bool checksum_correct() const
	{
		return smbios_checksum_correct(anchor_string, length);
	}

} __attribute__((packed));


struct Genode::Smbios_entry_point
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

	bool length_valid() const
	{
		return length <= MAX_LENGTH;
	}

	bool checksum_correct() const
	{
		return smbios_checksum_correct(anchor_string, length);
	}

	bool interm_checksum_correct() const
	{
		return smbios_checksum_correct(interm_anchor_string, INTERM_LENGTH);
	}

	Dmi_entry_point const &dmi_ep() const
	{
		return *(Dmi_entry_point *)&interm_anchor_string;
	}

} __attribute__((packed));


namespace Genode::Smbios_table
{
	template <typename PHY_MEM_FUNC,
	          typename EP_FUNC>
	bool smbios_3(addr_t       const  anchor,
	              addr_t       const  ep_phy,
	              PHY_MEM_FUNC const &phy_mem,
	              EP_FUNC      const &handle_ep)
	{
		if (memcmp((char *)anchor, "_SM3_", 5)) {
			return false;
		}
		Smbios_3_entry_point const &ep { *(Smbios_3_entry_point *)
			phy_mem(ep_phy, sizeof(Smbios_3_entry_point)) };

		if (!ep.length_valid()) {
			warning("SMBIOS 3 entry point has bad length");
			return false;
		}
		if (!ep.checksum_correct()) {
			warning("SMBIOS 3 entry point has bad checksum");
			return false;
		}
		if (ep.struct_table_addr > (uint64_t)(~(addr_t)0)) {
			warning("SMBIOS 3 entry point has bad structure-table address");
			return false;
		}
		log("SMBIOS 3 table (entry point: ", Hex(anchor), " structures: ", Hex(ep.struct_table_addr), ")");
		handle_ep(ep);
		return true;
	}

	template <typename PHY_MEM_FUNC,
	          typename EP_FUNC>
	bool smbios(addr_t       const  anchor,
	            addr_t       const  ep_phy,
	            PHY_MEM_FUNC const &phy_mem,
	            EP_FUNC      const &handle_ep)
	{
		if (memcmp((char *)anchor, "_SM_",  4)) {
			return false;
		}
		Smbios_entry_point const &ep { *(Smbios_entry_point *)
			phy_mem(ep_phy, sizeof(Smbios_entry_point)) };

		if (!ep.length_valid()) {
			warning("SMBIOS entry point has bad length");
			return false;
		}
		if (!ep.checksum_correct()) {
			warning("SMBIOS entry point has bad checksum");
			return false;
		}
		if (String<6>((char const *)&ep.interm_anchor_string) != "_DMI_") {
			warning("SMBIOS entry point has bad intermediate anchor string");
			return false;
		}
		if (!ep.interm_checksum_correct()) {
			warning("SMBIOS entry point has bad intermediate checksum");
			return false;
		}
		log("SMBIOS table (entry point: ", Hex(anchor), " structures: ", Hex(ep.struct_table_addr), ")");
		handle_ep(ep);
		return true;
	}

	template <typename PHY_MEM_FUNC,
	          typename EP_FUNC>
	bool dmi(addr_t       const  anchor,
	         addr_t       const  ep_phy,
	         PHY_MEM_FUNC const &phy_mem,
	         EP_FUNC      const &handle_ep)
	{
		if (memcmp((char *)anchor, "_DMI_", 5)) {
			return false;
		}
		Dmi_entry_point const &ep { *(Dmi_entry_point *)
			phy_mem(ep_phy, sizeof(Dmi_entry_point)) };

		if (!ep.checksum_correct()) {
			warning("DMI entry point has bad checksum");
			return false;
		}
		log("DMI table (entry point: ", Hex(anchor), " structures: ", Hex(ep.struct_table_addr), ")");
		handle_ep(ep);
		return true;
	}

	template <typename PHY_MEM_FUNC,
	          typename SMBIOS_3_FUNC,
	          typename SMBIOS_FUNC,
	          typename DMI_FUNC>
	void from_scan(PHY_MEM_FUNC  const &phy_mem,
	               SMBIOS_3_FUNC const &handle_smbios_3_ep,
	               SMBIOS_FUNC   const &handle_smbios_ep,
	               DMI_FUNC      const &handle_dmi_ep)
	{
		enum { SCAN_BASE_PHY    = 0xf0000 };
		enum { SCAN_SIZE        = 0x10000 };
		enum { SCAN_SIZE_SMBIOS =  0xfff0 };
		enum { SCAN_STEP        =    0x10 };

		addr_t const scan_base { (addr_t)phy_mem(SCAN_BASE_PHY, SCAN_SIZE) };
		try {
			addr_t const scan_end        { scan_base + SCAN_SIZE };
			size_t const scan_end_smbios { scan_base + SCAN_SIZE_SMBIOS };

			for (addr_t curr { scan_base }; curr < scan_end_smbios; curr += SCAN_STEP ) {
				if (smbios_3(curr, SCAN_BASE_PHY + (curr - scan_base), phy_mem, handle_smbios_3_ep)) {
					return;
				}
			}
			for (addr_t curr { scan_base }; curr < scan_end_smbios; curr += SCAN_STEP ) {
				if (smbios(curr, SCAN_BASE_PHY + (curr - scan_base), phy_mem, handle_smbios_ep)) {
					return;
				}
			}
			for (addr_t curr { scan_base }; curr < scan_end; curr += SCAN_STEP ) {
				if (dmi(curr, SCAN_BASE_PHY + (curr - scan_base), phy_mem, handle_dmi_ep)) {
					return;
				}
			}
		} catch (...) { }
	}

	template <typename PHY_MEM_FUNC,
	          typename SMBIOS_3_FUNC,
	          typename SMBIOS_FUNC,
	          typename DMI_FUNC>
	void from_pointer(addr_t        const  table_phy,
	                  PHY_MEM_FUNC  const &phy_mem,
	                  SMBIOS_3_FUNC const &handle_smbios_3_ep,
	                  SMBIOS_FUNC   const &handle_smbios_ep,
	                  DMI_FUNC      const &handle_dmi_ep)
	{
		addr_t const anchor { (addr_t)phy_mem(table_phy, 5) };
		if (smbios_3(anchor, table_phy, phy_mem, handle_smbios_3_ep)) {
			return;
		}
		if (smbios(anchor, table_phy, phy_mem, handle_smbios_ep)) {
			return;
		}
		dmi(anchor, table_phy, phy_mem, handle_dmi_ep);
	}
};

#endif /* _OS__SMBIOS_H_ */
