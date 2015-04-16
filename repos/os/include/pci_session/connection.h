/*
 * \brief  Connection to PCI service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PCI_SESSION__CONNECTION_H_
#define _INCLUDE__PCI_SESSION__CONNECTION_H_

#include <pci_session/client.h>
#include <base/connection.h>

namespace Pci { struct Connection; }


struct Pci::Connection : Genode::Connection<Session>, Session_client
{
	Connection()
	:
		Genode::Connection<Session>(session("ram_quota=12K")),
		Session_client(cap())
	{ }
};

#endif /* _INCLUDE__PCI_SESSION__CONNECTION_H_ */
