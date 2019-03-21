/*
 * \brief   ACPI RSDP structure 
 * \author  Alexander Boettcher
 * \date    2017-08-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__ACPI_RSDP_H
#define _SRC__LIB__HW__ACPI_RSDP_H

#include <base/fixed_stdint.h>

namespace Hw {
	struct Acpi_rsdp;
}

struct Hw::Acpi_rsdp
{
	Genode::uint64_t signature;
	Genode::uint8_t  checksum;
	char             oem[6];
	Genode::uint8_t  revision;
	Genode::uint32_t rsdt;
	Genode::uint32_t length;
	Genode::uint64_t xsdt;
	Genode::uint32_t reserved;

	bool valid() {
		const char sign[] = "RSD PTR ";
		return signature == *(Genode::uint64_t *)sign;
	}
} __attribute__((packed));

#endif /* _SRC__LIB__HW__ACPI_RSDP_H */
