/**
 * \brief  PCI fixup calls to execute
 * \author Josef Soentgen
 * \date   2022-02-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PCI_FIXUPS_H_
#define _LX_EMUL__PCI_FIXUPS_H_

static const char * lx_emul_pci_final_fixups[] = {
	"__pci_fixup_final_quirk_usb_early_handoff",
	"END_OF_PCI_FIXUPS"
};

#endif /* _LX_EMUL__PCI_FIXUPS_H_ */
