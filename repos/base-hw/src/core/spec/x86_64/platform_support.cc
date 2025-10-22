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


void Core::Platform::_init_additional_platform_info(Generator &g)
{
	if (_boot_info().plat_info.efi_system_table != 0) {
		g.node("efi-system-table", [&] {
			g.attribute("address", String<32>(Hex(_boot_info().plat_info.efi_system_table)));
		});
	}
	g.node("acpi", [&] {
		uint32_t const revision = _boot_info().plat_info.acpi_rsdp.revision;
		uint32_t const rsdt     = _boot_info().plat_info.acpi_rsdp.rsdt;
		uint64_t const xsdt     = _boot_info().plat_info.acpi_rsdp.xsdt;

		if (revision && (rsdt || xsdt)) {
			g.attribute("revision", revision);
			if (rsdt)
				g.attribute("rsdt", String<32>(Hex(rsdt)));

			if (xsdt)
				g.attribute("xsdt", String<32>(Hex(xsdt)));
		}
	});
	g.node("boot", [&] {
		g.node("framebuffer", [&] {
			Hw::Framebuffer const &boot_fb = _boot_info().plat_info.framebuffer;
			g.attribute("phys",   String<32>(Hex(boot_fb.addr)));
			g.attribute("width",  boot_fb.width);
			g.attribute("height", boot_fb.height);
			g.attribute("bpp",    boot_fb.bpp);
			g.attribute("type",   boot_fb.type);
			g.attribute("pitch",  boot_fb.pitch);
		});
	});
	g.node("hardware", [&] {
		g.node("features", [&] {
			g.attribute("svm", Hw::Virtualization_support::has_svm());
			g.attribute("vmx", Hw::Virtualization_support::has_vmx());
		});
		g.node("tsc", [&] {
			g.attribute("invariant", Hw::Tsc::invariant_tsc());
			g.attribute("freq_khz", _boot_info().plat_info.tsc_freq_khz);
		});
	});
}


Bit_allocator<64> &msi_allocator()
{
	static Bit_allocator<64> msi_allocator;
	return msi_allocator;
}


bool Core::Platform::alloc_msi_vector(addr_t &address, addr_t &value)
{
	return msi_allocator().alloc().convert<bool>(
		[&] (addr_t const v) {
			address = Hw::Cpu_memory_map::lapic_phys_base();
			value = Board::Local_interrupt_controller::IPI - 1 - v;
			return true;
		},
		[&] (Bit_allocator<64>::Error) { return false; });
}


void Core::Platform::free_msi_vector(addr_t, addr_t value)
{
	msi_allocator().free(Board::Local_interrupt_controller::IPI - 1 - value);
}


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:
	X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate)
{ }
