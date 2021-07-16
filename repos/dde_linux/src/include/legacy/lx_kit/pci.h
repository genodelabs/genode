/*
 * \brief  PCI backend
 * \author Josef Soentgen
 * \date   2017-02-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__PCI_H_
#define _LX_KIT__PCI_H_

/* Genode includes */
#include <base/env.h>
#include <base/allocator.h>


namespace Lx {

	void pci_init(Genode::Env&, Genode::Ram_allocator&, Genode::Allocator&);
}

#endif /* _LX_KIT__PCI_H_ */
