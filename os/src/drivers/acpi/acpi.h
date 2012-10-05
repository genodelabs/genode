/*
 * \brief  Interface to ACPI
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-02-25
 */

 /*
  * Copyright (C) 2009-2012 Genode Labs GmbH
  *
  * This file is part of the Genode OS framework, which is distributed
  * under the terms of the GNU General Public License version 2.
  */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <base/thread.h>
#include <pci_session/capability.h>

class Acpi
{
	public:

		/**
		 * Rewrite PCI-config space with GSIs found in ACPI tables
		 */
		static void rewrite_irq(Pci::Session_capability &session);

		/**
		 * Return override GSI for IRQ
		 */
		static unsigned override(unsigned irq, unsigned *mode);
};

#endif /* _ACPI_H_ */

