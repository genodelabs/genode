/*
 * \brief  Lx_emul backend for PCI devices
 * \author Stefan Kalkowski
 * \date   2022-05-23
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <pci/types.h>
#include <lx_emul/init.h>
#include <lx_emul/pci.h>
#include <lx_kit/env.h>

extern "C" void lx_emul_pci_enable(const char * const name)
{
	using namespace Lx_kit;

	env().devices.for_each([&] (Device & d) {
		if (d.name() == name) d.enable(); });
};


extern "C" void
lx_emul_pci_for_each_resource(const char * const name, void * dev,
                              lx_emul_add_resource_callback_t fn)
{
	using namespace Lx_kit;
	using namespace Genode;
	using namespace Pci;

	env().devices.for_each([&] (Device & d) {
		if (d.name() != name)
			return;

		d.for_each_io_mem([&] (Device::Io_mem & io_mem) {
			fn(dev, io_mem.pci_bar, io_mem.addr, io_mem.size, 0);
		});

		d.for_each_io_port([&] (Device::Io_port & io_port) {
			fn(dev, io_port.pci_bar, io_port.addr, io_port.size, 1);
		});
	});
}


extern "C" void
lx_emul_pci_for_each_device(void * bus, lx_emul_add_device_callback_t fn)
{
	using namespace Lx_kit;
	using namespace Genode;
	using namespace Pci;

	unsigned num = 0;
	env().devices.for_each([&] (Device & d) {
		unsigned irq = 0;
		d.for_each_irq([&] (Device::Irq & i) {
			if (!irq) irq = i.number.value; });

		d.for_pci_config([&] (Device::Pci_config & cfg) {
			fn(bus, num++, d.name().string(), cfg.vendor_id, cfg.device_id,
			   cfg.sub_v_id, cfg.sub_d_id, cfg.class_code, cfg.rev, irq);
		});
	});
}


static const char * lx_emul_pci_final_fixups[] = {
	"__pci_fixup_final_quirk_usb_early_handoff",
	"END_OF_PCI_FIXUPS"
};


extern "C" __attribute__((weak)) int inhibit_pci_fixup(char const *)
{
	return 0;
}


extern "C" void lx_emul_register_pci_fixup(void (*fn)(struct pci_dev*), const char * name)
{
	if (inhibit_pci_fixup(name))
		return;

	for (unsigned i = 0; i < (sizeof(lx_emul_pci_final_fixups) / sizeof(char*));
	     i++) {
		if (Genode::strcmp(name, lx_emul_pci_final_fixups[i]) == 0) {
			Lx_kit::env().pci_fixup_calls.add(fn);
			return;
		}
	}
	Genode::error(__func__, " ignore unkown PCI fixup '", name, "'");
}


extern "C" void lx_emul_execute_pci_fixup(struct pci_dev *pci_dev)
{
	Lx_kit::env().pci_fixup_calls.execute(pci_dev);
}
