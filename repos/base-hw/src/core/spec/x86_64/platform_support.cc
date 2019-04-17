/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <bios_data_area.h>
#include <platform.h>
#include <kernel/kernel.h>
#include <map_local.h>

using namespace Genode;

void Platform::_init_additional_platform_info(Xml_generator &xml)
{
	xml.node("acpi", [&] () {
		uint32_t const revision = _boot_info().acpi_rsdp.revision;
		uint32_t const rsdt     = _boot_info().acpi_rsdp.rsdt;
		uint64_t const xsdt     = _boot_info().acpi_rsdp.xsdt;

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
			Hw::Framebuffer const &boot_fb = _boot_info().framebuffer;
			xml.attribute("phys",   String<32>(Hex(boot_fb.addr)));
			xml.attribute("width",  boot_fb.width);
			xml.attribute("height", boot_fb.height);
			xml.attribute("bpp",    boot_fb.bpp);
			xml.attribute("type",   boot_fb.type);
			xml.attribute("pitch",  boot_fb.pitch);
		});
	});
	xml.node("hardware", [&] () {
		xml.node("features", [&] () {
			xml.attribute("svm", false);
			xml.attribute("vmx", false);
		});
	});
}


void Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                              unsigned polarity) {
	Kernel::pic().ioapic.setup_irq_mode(irq_number, trigger, polarity); }


bool Platform::get_msi_params(addr_t, addr_t &, addr_t &, unsigned &) {
	return false; }


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate) {}
