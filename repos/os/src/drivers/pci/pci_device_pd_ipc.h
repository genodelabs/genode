/*
 * \brief  IPC interface between pci_drv and pci_device_pd
 * \author Alexander Boettcher
 * \date   2013-02-15
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PCI_DEVICE_PD_IPC_H_
#define _PCI_DEVICE_PD_IPC_H_

#include <base/rpc_server.h>

#include <io_mem_session/capability.h>
#include <ram_session/ram_session.h>

namespace Pci {

		struct Device_pd : Genode::Session
		{
			static const char *service_name() { return "PCI_DEV_PD"; }

			GENODE_RPC(Rpc_attach_dma_mem, void, attach_dma_mem,
			           Genode::Ram_dataspace_capability);
			GENODE_RPC(Rpc_assign_pci, void, assign_pci,
			           Genode::Io_mem_dataspace_capability);

			GENODE_RPC_INTERFACE(Rpc_attach_dma_mem, Rpc_assign_pci);
		};

		struct Device_pd_client : Genode::Rpc_client<Device_pd>
		{
				Device_pd_client(Capability<Device_pd> cap)
				:
					Rpc_client<Device_pd>(cap) { }

				void attach_dma_mem(Genode::Ram_dataspace_capability cap) {
					call<Rpc_attach_dma_mem>(cap); }

				void assign_pci(Genode::Io_mem_dataspace_capability cap) {
					call<Rpc_assign_pci>(cap); }
		};

		struct Device_pd_component : Genode::Rpc_object<Device_pd,
		                                                Device_pd_component>
		{
			void attach_dma_mem(Genode::Ram_dataspace_capability);
			void assign_pci(Genode::Io_mem_dataspace_capability);
		};

}
#endif /* _PCI_DEVICE_PD_IPC_H_ */
