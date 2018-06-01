/*
 * \brief  PCI discovery information
 * \author Norman Feske
 * \date   2018-06-01
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__PCI_INFO_H_
#define _MODEL__PCI_INFO_H_

#include "types.h"

namespace Sculpt { struct Pci_info; }

struct Sculpt::Pci_info
{
	bool wifi_present = false;
};

#endif /* _MODEL__PCI_INFO_H_ */
