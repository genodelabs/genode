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

#include <util/attempt.h>
#include <base/exception.h>
#include <base/stdint.h>
#include <base/signal.h>
#include <base/quota_guard.h>
#include <dataspace/capability.h>

namespace Genode { struct Region_map; }


struct Genode::Region_map : Interface
{
	/**
	 * Fault state of region map
	 *
	 * If a thread accesses a location outside the regions attached to its
	 * address space, a fault occurs and gets signalled to the registered fault
	 * handler. The fault handler, in turn needs the information about the
	 * fault address and fault type to resolve the fault. This information is
	 * represented by this structure.
	 */
	struct Fault
	{
		enum class Type { NONE, READ, WRITE, EXEC };

		Type   type; /* type of occurred fault */
		addr_t addr; /* fault address unless fault is 'NONE' */
	};

	struct Range { addr_t start; size_t num_bytes; };

	/**
	 * Attributes for 'attach'
	 */
	struct Attr
	{
		size_t size;      /* size of the mapping, or 0 for the whole dataspace */
		addr_t offset;    /* page-aligned offset in dataspace */
		bool   use_at;
		addr_t at;        /* designated start of region if 'use_at' is true */
		bool   executable;
		bool   writeable;
	};

	enum class Attach_error { OUT_OF_RAM, OUT_OF_CAPS, REGION_CONFLICT, INVALID_DATASPACE };

	using Attach_result = Attempt<Range, Attach_error>;

	/**
	 * Map dataspace into region map
	 *
	 * \param ds    capability of dataspace to map
	 * \param attr  mapping attributes
	 * \return      address range of mapping within region map
	 */
	virtual Attach_result attach(Dataspace_capability ds, Attr const &attr) = 0;

	/**
	 * Remove region from local address space
	 */
	virtual void detach(addr_t) = 0;

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
	 * Request current fault state of region map
	 */
	virtual Fault fault() = 0;

	/**
	 * Return dataspace representation of region map
	 */
	virtual Dataspace_capability dataspace() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_attach, Attach_result, attach, Dataspace_capability, Attr const &);
	GENODE_RPC(Rpc_detach, void, detach, addr_t);
	GENODE_RPC(Rpc_fault_handler, void, fault_handler, Signal_context_capability);
	GENODE_RPC(Rpc_fault, Fault, fault);
	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);

	GENODE_RPC_INTERFACE(Rpc_attach, Rpc_detach, Rpc_fault_handler, Rpc_fault,
	                     Rpc_dataspace);
};

#endif /* _INCLUDE__REGION_MAP__REGION_MAP_H_ */
