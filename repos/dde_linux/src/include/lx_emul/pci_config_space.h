/*
 * \brief  Lx_emul support for accessing PCI(e) config space
 * \author Josef Soentgen
 * \date   2022-01-18
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PCI_CONFIG_SPACE_H_
#define _LX_EMUL__PCI_CONFIG_SPACE_H_

#ifdef __cplusplus
extern "C" {
#endif

int lx_emul_pci_read_config(unsigned bus, unsigned devfn, unsigned reg, unsigned len, unsigned *val);
int lx_emul_pci_write_config(unsigned bus, unsigned devfn, unsigned reg, unsigned len, unsigned val);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__PCI_CONFIG_SPACE_H_ */
