/*
 * \brief  Region map interface
 * \author Norman Feske
 * \date   2006-05-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGION_MAP__REGION_MAP_H_
#define _INCLUDE__REGION_MAP__REGION_MAP_H_

#include <base/exception.h>
#include <base/stdint.h>
#include <base/signal.h>
#include <dataspace/capability.h>
#include <thread/capability.h>

namespace Genode { struct Region_map; }


struct Genode::Region_map
{
	/**
	 * State of region map
	 *
	 * If a thread accesses a location outside the regions attached to its
	 * address space, a fault occurs and gets signalled to the registered fault
	 * handler. The fault handler, in turn needs the information about the
	 * fault address and fault type to resolve the fault. This information is
	 * represented by this structure.
	 */
	struct State
	{
		enum Fault_type { READY, READ_FAULT, WRITE_FAULT, EXEC_FAULT };

		/**
		 * Type of occurred fault
		 */
		Fault_type type = READY;

		/**
		 * Fault address
		 */
		addr_t addr = 0;

		/**
		 * Default constructor
		 */
		State() { }

		/**
		 * Constructor
		 */
		State(Fault_type fault_type, addr_t fault_addr)
		: type(fault_type), addr(fault_addr) { }
	};


	/**
	 * Helper for tranferring the bit representation of a pointer as RPC
	 * argument.
	 */
	class Local_addr
	{
		private:

			void *_ptr = nullptr;

		public:

			template <typename T>
			Local_addr(T ptr) : _ptr((void *)ptr) { }

			Local_addr() { }

			template <typename T>
			operator T () { return (T)_ptr; }
	};


	/*********************
	 ** Exception types **
	 *********************/

	class Attach_failed     : public Exception     { };
	class Invalid_args      : public Attach_failed { };
	class Invalid_dataspace : public Attach_failed { };
	class Region_conflict   : public Attach_failed { };
	class Out_of_metadata   : public Attach_failed { };

	class Invalid_thread    : public Exception { };
	class Unbound_thread    : public Exception { };


	/**
	 * Map dataspace into local address space
	 *
	 * \param ds               capability of dataspace to map
	 * \param size             size of the locally mapped region
	 *                         default (0) is the whole dataspace
	 * \param offset           start at offset in dataspace (page-aligned)
	 * \param use_local_addr   if set to true, attach the dataspace at
	 *                         the specified 'local_addr'
	 * \param local_addr       local destination address
	 * \param executable       if the mapping should be executable
	 *
	 * \throw Attach_failed    if dataspace or offset is invalid,
	 *                         or on region conflict
	 * \throw Out_of_metadata  if meta-data backing store is exhausted
	 *
	 * \return                 local address of mapped dataspace
	 *
	 */
	virtual Local_addr attach(Dataspace_capability ds,
	                          size_t size = 0, off_t offset = 0,
	                          bool use_local_addr = false,
	                          Local_addr local_addr = (void *)0,
	                          bool executable = false) = 0;

	/**
	 * Shortcut for attaching a dataspace at a predefined local address
	 */
	Local_addr attach_at(Dataspace_capability ds, addr_t local_addr,
	                     size_t size = 0, off_t offset = 0) {
		return attach(ds, size, offset, true, local_addr); }

	/**
	 * Shortcut for attaching a dataspace executable at a predefined local address
	 */
	Local_addr attach_executable(Dataspace_capability ds, addr_t local_addr,
	                             size_t size = 0, off_t offset = 0) {
		return attach(ds, size, offset, true, local_addr, true); }

	/**
	 * Remove region from local address space
	 */
	virtual void detach(Local_addr local_addr) = 0;

	/**
	 * Register signal handler for region-manager faults
	 *
	 * On Linux, this signal is never delivered because page-fault handling
	 * is performed by the Linux kernel. On microkernel platforms,
	 * unresolvable page faults (traditionally called segmentation fault)
	 * will result in the delivery of the signal.
	 */
	virtual void fault_handler(Signal_context_capability handler) = 0;

	/**
	 * Request current state of region map
	 */
	virtual State state() = 0;

	/**
	 * Return dataspace representation of region map
	 */
	virtual Dataspace_capability dataspace() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_attach, Local_addr, attach,
	                 GENODE_TYPE_LIST(Invalid_dataspace, Region_conflict,
	                                  Out_of_metadata, Invalid_args),
	                 Dataspace_capability, size_t, off_t, bool, Local_addr, bool);
	GENODE_RPC(Rpc_detach, void, detach, Local_addr);
	GENODE_RPC(Rpc_fault_handler, void, fault_handler, Signal_context_capability);
	GENODE_RPC(Rpc_state, State, state);
	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);

	GENODE_RPC_INTERFACE(Rpc_attach, Rpc_detach, Rpc_fault_handler, Rpc_state,
	                     Rpc_dataspace);
};

#endif /* _INCLUDE__REGION_MAP__REGION_MAP_H_ */
