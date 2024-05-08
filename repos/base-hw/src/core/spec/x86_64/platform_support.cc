/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \author  Benjamin Lamowski
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bios_data_area.h>
#include <platform.h>
#include <kernel/cpu.h>
#include <map_local.h>
#include <hw/spec/x86_64/x86_64.h>

using namespace Core;


void Platform::_init_additional_platform_info(Xml_generator &xml)
{
	if (_boot_info().plat_info.efi_system_table != 0) {
		xml.node("efi-system-table", [&] () {
			xml.attribute("address", String<32>(Hex(_boot_info().plat_info.efi_system_table)));
		});
	}
	xml.node("acpi", [&] () {
		uint32_t const revision = _boot_info().plat_info.acpi_rsdp.revision;
		uint32_t const rsdt     = _boot_info().plat_info.acpi_rsdp.rsdt;
		uint64_t const xsdt     = _boot_info().plat_info.acpi_rsdp.xsdt;

		if (revision && (rsdt || xsdt)) {
			xml.attribute("revision", revision);
			if (rsdt)
				xml.attribute("rsdt", String<32>(Hex(rsdt)));

			if (xsdt)
				xml.attribute("xsdt", String<32>(Hex(xsdt)));
		}
	});
	xml.node("boot", [&] () {
		xml.node("framebuffer", [&] () {
			Hw::Framebuffer const &boot_fb = _boot_info().plat_info.framebuffer;
			xml.attribute("phys",   String<32>(Hex(boot_fb.addr)));
			xml.attribute("width",  boot_fb.width);
			xml.attribute("height", boot_fb.height);
			xml.attribute("bpp",    boot_fb.bpp);
			xml.attribute("type",   boot_fb.type);
			xml.attribute("pitch",  boot_fb.pitch);
		});
	});
	xml.node("hardware", [&]() {
		xml.node("features", [&] () {
			xml.attribute("svm", Hw::Virtualization_support::has_svm());
			xml.attribute("vmx", Hw::Virtualization_support::has_vmx());
		});
		xml.node("tsc", [&]() {
			xml.attribute("invariant", Hw::Lapic::invariant_tsc());
			xml.attribute("freq_khz", Hw::Lapic::tsc_freq());
		});
	});
}


Bit_allocator<64> &msi_allocator()
{
	static Bit_allocator<64> msi_allocator;
	return msi_allocator;
}


bool Platform::alloc_msi_vector(addr_t & address, addr_t & value)
{
	try {
		address = Hw::Cpu_memory_map::lapic_phys_base();
		value   = Board::Pic::IPI - 1 - msi_allocator().alloc();
	return true;
	} catch(...) {}
	return false;
}


void Platform::free_msi_vector(addr_t, addr_t value)
{
	msi_allocator().free(Board::Pic::IPI - 1 - value);
}


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:
	X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate)
{ }
