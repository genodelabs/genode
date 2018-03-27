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

#include <util/xml_generator.h>

using namespace Genode;


void Platform::_init_additional()
{
	/* export x86 platform specific infos */

	unsigned const  pages    = 1;
	size_t   const  rom_size = pages << get_page_size_log2();
	void           *phys_ptr = nullptr;
	void           *virt_ptr = nullptr;
	const char     *rom_name = "platform_info";

	if (!ram_alloc()->alloc(get_page_size(), &phys_ptr)) {
		error("could not setup platform_info ROM - ram allocation error");
		return;
	}

	if (!region_alloc()->alloc(rom_size, &virt_ptr)) {
		error("could not setup platform_info ROM - region allocation error");
		ram_alloc()->free(phys_ptr);
		return;
	}

	addr_t const phys_addr = reinterpret_cast<addr_t>(phys_ptr);
	addr_t const virt_addr = reinterpret_cast<addr_t>(virt_ptr);

	if (!map_local(phys_addr, virt_addr, pages, Hw::PAGE_FLAGS_KERN_DATA)) {
		error("could not setup platform_info ROM - map error");
		region_alloc()->free(virt_ptr);
		ram_alloc()->free(phys_ptr);
		return;
	}

	Genode::Xml_generator xml(reinterpret_cast<char *>(virt_addr),
	                          rom_size, rom_name, [&] ()
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
	});

	if (!unmap_local(virt_addr, pages)) {
		error("could not setup platform_info ROM - unmap error");
		return;
	}

	region_alloc()->free(virt_ptr);

	_rom_fs.insert(
		new (core_mem_alloc()) Rom_module(phys_addr, rom_size, rom_name));
}


void Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                              unsigned polarity) {
	Kernel::pic()->ioapic.setup_irq_mode(irq_number, trigger, polarity); }


bool Platform::get_msi_params(addr_t, addr_t &, addr_t &, unsigned &) {
	return false; }


Board::Serial::Serial(addr_t, size_t, unsigned baudrate)
:X86_uart(Bios_data_area::singleton()->serial_port(), 0, baudrate) {}
