/*
 * \brief  AHCI driver
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \author Martin Stein    <Martin.Stein@genode-labs.com>
 * \date   2006-08-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _AHCI_DRIVER_H_
#define _AHCI_DRIVER_H_

/* Genode includes */
#include <pci_session/connection.h>

/* local includes */
#include <ahci_driver_base.h>

/**
 * Helper class for AHCI driver to ensure specific member-initialization order
 */
class Ahci_pci_connection {

	protected:

		Pci::Connection _pci;
};

/**
 * AHCI driver
 */
class Ahci_driver : public Ahci_pci_connection,
                    public Ahci_driver_base
{
	public:

		/**
		 * Constructor
		 */
		Ahci_driver() : Ahci_driver_base(Ahci_device::probe(_pci)) { }
};

#endif /* _AHCI_DRIVER_H_ */

