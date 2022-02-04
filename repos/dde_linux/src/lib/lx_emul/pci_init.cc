/*
 * \brief  Lx_emul backend for PCI fixup calls
 * \author Josef Soentgen
 * \date   2022-02-04
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <lx_kit/env.h>
#include <lx_emul/init.h>

#include <lx_emul/pci_fixups.h>


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
