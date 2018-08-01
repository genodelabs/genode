/*
 * \brief   ACPI structures and parsing
 * \author  Alexander Boettcher
 * \date    2018-04-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__ACPI_H_
#define _SRC__LIB__HW__SPEC__X86_64__ACPI_H_

#include <base/fixed_stdint.h>

namespace Hw {
	struct Acpi_generic;
	struct Apic_madt;

	template <typename FUNC>
	void for_each_rsdt_entry(Hw::Acpi_generic &, FUNC);

	template <typename FUNC>
	void for_each_xsdt_entry(Hw::Acpi_generic &, FUNC);

	template <typename FUNC>
	void for_each_apic_struct(Acpi_generic &, FUNC);
}

/* ACPI spec 5.2.6 */
struct Hw::Acpi_generic
{
	Genode::uint8_t  signature[4];
	Genode::uint32_t size;
	Genode::uint8_t  rev;
	Genode::uint8_t  checksum;
	Genode::uint8_t  oemid[6];
	Genode::uint8_t  oemtabid[8];
	Genode::uint32_t oemrev;
	Genode::uint8_t  creator[4];
	Genode::uint32_t creator_rev;

} __attribute__((packed));

struct Hw::Apic_madt
{
	enum { LAPIC = 0, IO_APIC = 1 };

	Genode::uint8_t type;
	Genode::uint8_t length;

	Apic_madt *next() const { return reinterpret_cast<Apic_madt *>((Genode::uint8_t *)this + length); }

	struct Ioapic : Genode::Mmio {

		struct Id       : Register <0x02,  8> { };
		struct Paddr    : Register <0x04, 32> { };
		struct Gsi_base : Register <0x08, 32> { };

		Ioapic(Apic_madt const * a) : Mmio(reinterpret_cast<Genode::addr_t>(a)) { }
	};

	struct Lapic : Genode::Mmio {

		struct Flags    : Register <0x04, 32> { enum { ENABLED = 1 }; };

		Lapic(Apic_madt const * a) : Mmio(reinterpret_cast<Genode::addr_t>(a)) { }
	};

} __attribute__((packed));

template <typename FUNC>
void Hw::for_each_rsdt_entry(Hw::Acpi_generic &rsdt, FUNC fn)
{
	if (Genode::memcmp(rsdt.signature, "RSDT", 4))
		return;

	typedef Genode::uint32_t entry_t;

	unsigned const table_size  = rsdt.size;
	unsigned const entry_count = (table_size - sizeof(rsdt)) / sizeof(entry_t);

	entry_t * entries = reinterpret_cast<entry_t *>(&rsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}

template <typename FUNC>
void Hw::for_each_xsdt_entry(Hw::Acpi_generic &xsdt, FUNC fn)
{
	if (Genode::memcmp(xsdt.signature, "XSDT", 4))
		return;

	typedef Genode::uint64_t entry_t;

	unsigned const table_size  = xsdt.size;
	unsigned const entry_count = (table_size - sizeof(xsdt)) / sizeof(entry_t);

	entry_t * entries = reinterpret_cast<entry_t *>(&xsdt + 1);
	for (unsigned i = 0; i < entry_count; i++)
		fn(entries[i]);
}

template <typename FUNC>
void Hw::for_each_apic_struct(Hw::Acpi_generic &apic_madt, FUNC fn)
{
	if (Genode::memcmp(apic_madt.signature, "APIC", 4))
		return;

	Apic_madt const * const first = reinterpret_cast<Apic_madt *>(&apic_madt.creator_rev + 3);
	Apic_madt const * const last  = reinterpret_cast<Apic_madt *>(reinterpret_cast<Genode::uint8_t *>(apic_madt.signature) + apic_madt.size);

	for (Apic_madt const * e = first; e < last ; e = e->next())
		fn(e);
}

#endif /* _SRC__LIB__HW__SPEC__X86_64__ACPI_H_ */
