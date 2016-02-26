/*
 * \brief  Protection domain (PD) session interface
 * \author Christian Helmuth
 * \date   2006-06-27
 *
 * A pd session represents the protection domain of a program.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PD_SESSION__PD_SESSION_H_
#define _INCLUDE__PD_SESSION__PD_SESSION_H_

#include <thread/capability.h>
#include <parent/capability.h>
#include <session/session.h>

namespace Genode {

	struct Pd_session : Session
	{
		static const char *service_name() { return "PD"; }

		virtual ~Pd_session() { }

		/**
		 * Bind thread to protection domain
		 *
		 * \param thread  capability of thread to bind
		 *
		 * \return        0 on success or negative error code
		 *
		 * After successful bind, the thread will execute inside this
		 * protection domain when started.
		 */
		virtual int bind_thread(Thread_capability thread) = 0;

		/**
		 * Assign parent to protection domain
		 *
		 * \param   parent  capability of parent interface
		 * \return  0 on success, or negative error code
		 */
		virtual int assign_parent(Parent_capability parent) = 0;


		/**
		 * Assign PCI device to a protection domain.
		 *
		 * \param   pci_config_space  virtual address of the 4K PCI config
		 *                            space extended memory of the device
		 * \param   bdf               bus/device/function of the PCI device
		 * \return  true              on success, or false in case of an error
		 */
		virtual bool assign_pci(addr_t pci_config_space, uint16_t bdf) = 0;

		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_bind_thread,   int, bind_thread,   Thread_capability);
		GENODE_RPC(Rpc_assign_parent, int, assign_parent, Parent_capability);
		GENODE_RPC(Rpc_assign_pci, bool, assign_pci, addr_t, uint16_t);

		GENODE_RPC_INTERFACE(Rpc_bind_thread, Rpc_assign_parent,
		                     Rpc_assign_pci);
	};
}

#endif /* _INCLUDE__PD_SESSION__PD_SESSION_H_ */
