/*
 * \brief  Generate XML report
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <os/reporter.h>

#include "util.h"
#include "bridge.h"

using Genode::Reporter;

extern void AcpiGenodeFreeIOMem(ACPI_PHYSICAL_ADDRESS const phys, ACPI_SIZE const size);

template <typename H, typename S, typename F, typename FSIZE>
void for_each_element(H const head, S *, F const &fn, FSIZE const &fn_size)
{
	for(S const * e = reinterpret_cast<S const * const>(head + 1);
	    e < reinterpret_cast<S const *>(reinterpret_cast<char const *>(head) + head->Header.Length);
	    e = reinterpret_cast<S const *>(reinterpret_cast<char const *>(e) + fn_size(e)))
	{
		fn(e);
	}
}

static void add_madt(ACPI_TABLE_MADT const * const madt,
                     Reporter::Xml_generator &xml)
{
	typedef ACPI_SUBTABLE_HEADER Madt_sub;

	using Genode::String;

	for_each_element(madt, (Madt_sub *) nullptr, [&](Madt_sub const * const s) {

		if (s->Type != ACPI_MADT_TYPE_INTERRUPT_OVERRIDE)
			return;

		typedef ACPI_MADT_INTERRUPT_OVERRIDE Irq;
		Irq const * const irq = reinterpret_cast<Irq const * const>(s);

		xml.node("irq_override", [&] () {
			xml.attribute("irq", irq->SourceIrq);
			xml.attribute("gsi", irq->GlobalIrq);
			xml.attribute("flags", String<16>(Genode::Hex(irq->IntiFlags)));
			xml.attribute("bus", irq->Bus);
		});
	}, [](Madt_sub const * const s) { return s->Length; });
}

static void add_mcfg(ACPI_TABLE_MCFG const * const mcfg,
                     Reporter::Xml_generator &xml)
{
	using namespace Genode;

	typedef ACPI_MCFG_ALLOCATION Mcfg_sub;

	for_each_element(mcfg, (Mcfg_sub *) nullptr, [&](Mcfg_sub const * const e) {

		/* bus_count * up to 32 devices * 8 function per device * 4k */
		uint32_t const bus_count  = e->EndBusNumber - e->StartBusNumber + 1;
		uint32_t const func_count = bus_count * 32 * 8;
		uint32_t const bus_start  = e->StartBusNumber * 32 * 8;

		xml.node("bdf", [&] () {
			xml.attribute("start", bus_start);
			xml.attribute("count", func_count);
			xml.attribute("base", String<24>(Hex(e->Address)));
		});

		/* force freeing I/O mem so that platform driver can use it XXX */
		AcpiGenodeFreeIOMem(e->Address, 0x1000UL * func_count);

	}, [](Mcfg_sub const * const e) { return sizeof(*e); });
}

static void add_dmar(ACPI_TABLE_DMAR const * const dmar_table,
                     Reporter::Xml_generator &xml)
{
	using Genode::String;
	using Genode::Hex;

	auto scope_length = [](ACPI_DMAR_DEVICE_SCOPE const * const e) {
		return e->Length; };

	auto scope_lambda = [&](ACPI_DMAR_DEVICE_SCOPE const * const e) {
		xml.node("scope", [&] () {
			xml.attribute("bus_start", e->Bus);
			xml.attribute("type", e->EntryType);

			unsigned const count = (e->Length < 6) ? 0 : ((e->Length - 6) / 2);

			ACPI_DMAR_PCI_PATH * path = ACPI_CAST_PTR(ACPI_DMAR_PCI_PATH, e + 1);
			for (unsigned i = 0; i < count; i++) {
				xml.node("path", [&] () {
					xml.attribute("dev", String<8>(Hex(path->Device)));
					xml.attribute("func", String<8>(Hex(path->Function)));
				});
			}
		});
	};

	for_each_element(dmar_table, (ACPI_DMAR_HEADER *) nullptr, [&](ACPI_DMAR_HEADER const * const e) {
		if (e->Type == ACPI_DMAR_TYPE_RESERVED_MEMORY) {
			ACPI_DMAR_RESERVED_MEMORY const * const dmar = ACPI_CAST_PTR (ACPI_DMAR_RESERVED_MEMORY, e);

			xml.node("rmrr", [&] () {
				xml.attribute("start", String<24>(Hex(dmar->BaseAddress)));
				xml.attribute("end"  , String<24>(Hex(dmar->EndAddress)));

				for_each_element(dmar, (ACPI_DMAR_DEVICE_SCOPE *) nullptr,
				                 scope_lambda, scope_length);
			});
		} else
		if (e->Type == ACPI_DMAR_TYPE_HARDWARE_UNIT) {
			ACPI_DMAR_HARDWARE_UNIT const * const drhd = ACPI_CAST_PTR (ACPI_DMAR_HARDWARE_UNIT, e);

			xml.node("drhd", [&] () {
				xml.attribute("phys", String<24>(Hex(drhd->Address)));
				xml.attribute("flags", String<4>(Hex(drhd->Flags)));
				xml.attribute("segment", String<8>(Hex(drhd->Segment)));

				for_each_element(drhd, (ACPI_DMAR_DEVICE_SCOPE *) nullptr,
				                 scope_lambda, scope_length);
			});
		}
	}, [](ACPI_DMAR_HEADER const * const e) { return e->Length; });
}

void Acpica::generate_report(Genode::Env &env, Bridge *pci_root_bridge)
{
	enum { REPORT_SIZE = 5 * 4096 };
	static Reporter acpi(env, "acpi", "acpi", REPORT_SIZE);
	acpi.enabled(true);

	Reporter::Xml_generator xml(acpi, [&] () {
		ACPI_TABLE_HEADER *header = nullptr;

		ACPI_STATUS status = AcpiGetTable((char *)ACPI_SIG_MADT, 0, &header);
		if (status == AE_OK)
			add_madt(reinterpret_cast<ACPI_TABLE_MADT *>(header), xml);

		status = AcpiGetTable((char *)ACPI_SIG_MCFG, 0, &header);
		if (status == AE_OK)
			add_mcfg(reinterpret_cast<ACPI_TABLE_MCFG *>(header), xml);

		for (unsigned instance = 1; ; instance ++) {

			status = AcpiGetTable(ACPI_STRING(ACPI_SIG_DMAR), instance,
			                      &header);
			if (status != AE_OK)
				break;

			add_dmar(reinterpret_cast<ACPI_TABLE_DMAR *>(header), xml);
		}

		if (pci_root_bridge)
			pci_root_bridge->generate(xml);
	});
}
