/*
 * \brief  Protection domain (PD) session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-06-27
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PD_SESSION__PD_SESSION_H_
#define _INCLUDE__PD_SESSION__PD_SESSION_H_

#include <base/exception.h>
#include <session/session.h>
#include <region_map/region_map.h>
#include <base/ram_allocator.h>

namespace Genode {
	struct Pd_session;
	struct Pd_session_client;
	struct Parent;
	struct Signal_context;
}


struct Genode::Pd_session : Session, Ram_allocator
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "PD"; }

	/*
	 * A PD session consumes a dataspace capability for the session-object
	 * allocation, a capability for the 'Native_pd' RPC interface, its
	 * session capability, and the RPC capabilities for the 3 contained
	 * region maps.
	 *
	 * Furthermore, we account for the dataspace capabilities allocated during
	 * the component bootstrapping.
	 */
	enum { CAP_QUOTA = 6 + 7 };

	typedef Pd_session_client Client;

	virtual ~Pd_session() { }

	/**
	 * Assign parent to protection domain
	 *
	 * \param   parent  capability of parent interface
	 */
	virtual void assign_parent(Capability<Parent> parent) = 0;

	/**
	 * Assign PCI device to PD
	 *
	 * The specified address has to refer to the locally mapped PCI
	 * configuration space of the device.
	 *
	 * This function is solely used on the NOVA kernel.
	 */
	virtual bool assign_pci(addr_t pci_config_memory_address, uint16_t bdf) = 0;

	/**
	 * Trigger eager insertion of page frames to page table within
	 * specified virtual range.
	 *
	 * If the used kernel don't support this feature, the operation will
	 * silently ignore the request.
	 *
	 * \param virt virtual address within the address space to start
	 * \param size the virtual size of the region
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	virtual void map(addr_t virt, addr_t size) = 0;

	/********************************
	 ** Support for the signal API **
	 ********************************/

	typedef Capability<Signal_source> Signal_source_capability;

	class Invalid_session       : public Exception { };
	class Undefined_ref_account : public Exception { };
	class Invalid_signal_source : public Exception { };

	/**
	 * Create a new signal source
	 *
	 * \return  a cap that acts as reference to the created source
	 *
	 * The signal source provides an interface to wait for incoming signals.
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 */
	virtual Signal_source_capability alloc_signal_source() = 0;

	/**
	 * Free a signal source
	 *
	 * \param cap  capability of the signal source to destroy
	 */
	virtual void free_signal_source(Signal_source_capability cap) = 0;

	/**
	 * Allocate signal context
	 *
	 * \param source  signal source that shall provide the new context
	 *
	 *
	 * \param imprint  opaque value that gets delivered with signals
	 *                 originating from the allocated signal-context capability
	 * \return new signal-context capability
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 * \throw Invalid_signal_source
	 */
	virtual Capability<Signal_context>
	alloc_context(Signal_source_capability source, unsigned long imprint) = 0;

	/**
	 * Free signal-context
	 *
	 * \param cap  capability of signal-context to release
	 */
	virtual void free_context(Capability<Signal_context> cap) = 0;

	/**
	 * Submit signals to the specified signal context
	 *
	 * \param context  signal destination
	 * \param cnt      number of signals to submit at once
	 *
	 * The 'context' argument does not necessarily belong to this PD session.
	 * Normally, it is a capability obtained from a potentially untrusted
	 * component. Because we cannot trust this capability, signals are not
	 * submitted by invoking 'cap' directly but by using it as argument to our
	 * trusted PD-session interface. Otherwise, a potential signal receiver
	 * could supply a capability with a blocking interface to compromise the
	 * nonblocking behaviour of the signal submission.
	 */
	virtual void submit(Capability<Signal_context> context, unsigned cnt = 1) = 0;


	/***********************************
	 ** Support for the RPC framework **
	 ***********************************/

	/**
	 * Allocate new RPC-object capability
	 *
	 * \param ep  entry point that will use this capability
	 *
	 * \throw Out_of_ram   if meta-data backing store is exhausted
	 * \throw Out_of_caps  if 'cap_quota' is exceeded
	 *
	 * \return new RPC capability
	 */
	virtual Native_capability alloc_rpc_cap(Native_capability ep) = 0;

	/**
	 * Free RPC-object capability
	 *
	 * \param cap  capability to free
	 */
	virtual void free_rpc_cap(Native_capability cap) = 0;


	/**************************************
	 ** Virtual address-space management **
	 **************************************/

	enum { LINKER_AREA_SIZE = 160*1024*1024UL };

	/**
	 * Return region map of the PD's virtual address space
	 */
	virtual Capability<Region_map> address_space() = 0;

	/**
	 * Return region map of the PD's stack area
	 */
	virtual Capability<Region_map> stack_area() = 0;

	/**
	 * Return region map of the PD's linker area
	 */
	virtual Capability<Region_map> linker_area() = 0;


	/*******************************************
	 ** Accounting for capability allocations **
	 *******************************************/

	/**
	 * Define reference account for the PD session
	 *
	 * \throw Invalid_session
	 */
	virtual void ref_account(Capability<Pd_session>) = 0;

	/**
	 * Transfer capability quota to another PD session
	 *
	 * \param to      receiver of quota donation
	 * \param amount  amount of quota to donate
	 *
	 * \throw Out_of_caps
	 * \throw Invalid_session
	 * \throw Undefined_ref_account
	 *
	 * Quota can only be transfered if the specified PD session is either the
	 * reference account for this session or vice versa.
	 */
	virtual void transfer_quota(Capability<Pd_session> to, Cap_quota amount) = 0;

	/**
	 * Return current capability-quota limit
	 */
	virtual Cap_quota cap_quota() const = 0;

	/**
	 * Return number of capabilities allocated from the session
	 */
	virtual Cap_quota used_caps() const = 0;

	/**
	 * Return amount of available capabilities
	 */
	Cap_quota avail_caps() const
	{
		return Cap_quota { cap_quota().value - used_caps().value };
	}


	/***********************************
	 ** RAM allocation and accounting **
	 ***********************************/

	/*
	 * Note that the 'Pd_session' inherits the 'Ram_allocator' interface,
	 * which comprises the actual allocation and deallocation operations.
	 */

	/**
	 * Transfer quota to another RAM session
	 *
	 * \param to      receiver of quota donation
	 * \param amount  amount of quota to donate
	 *
	 * \throw Out_of_ram
	 * \throw Invalid_session
	 * \throw Undefined_ref_account
	 *
	 * Quota can only be transfered if the specified PD session is either the
	 * reference account for this session or vice versa.
	 */
	virtual void transfer_quota(Capability<Pd_session> to, Ram_quota amount) = 0;

	/**
	 * Return current quota limit
	 */
	virtual Ram_quota ram_quota() const = 0;

	/**
	 * Return used quota
	 */
	virtual Ram_quota used_ram() const = 0;

	/**
	 * Return amount of available quota
	 */
	Ram_quota avail_ram() const { return { ram_quota().value - used_ram().value }; }


	/*****************************************
	 ** Access to kernel-specific interface **
	 *****************************************/

	/**
	 * Common base class of kernel-specific PD interfaces
	 */
	struct Native_pd : Interface { };

	/**
	 * Return capability to kernel-specific PD operations
	 */
	virtual Capability<Native_pd> native_pd() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_assign_parent, void, assign_parent, Capability<Parent>);
	GENODE_RPC(Rpc_assign_pci,    bool, assign_pci,    addr_t, uint16_t);
	GENODE_RPC_THROW(Rpc_map,     void, map,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps),
	                 addr_t, addr_t);

	GENODE_RPC_THROW(Rpc_alloc_signal_source, Signal_source_capability,
	                 alloc_signal_source,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps));
	GENODE_RPC(Rpc_free_signal_source, void, free_signal_source, Signal_source_capability);
	GENODE_RPC_THROW(Rpc_alloc_context, Capability<Signal_context>, alloc_context,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Invalid_signal_source),
	                 Signal_source_capability, unsigned long);
	GENODE_RPC(Rpc_free_context, void, free_context,
	           Capability<Signal_context>);
	GENODE_RPC(Rpc_submit, void, submit, Capability<Signal_context>, unsigned);

	GENODE_RPC_THROW(Rpc_alloc_rpc_cap, Native_capability, alloc_rpc_cap,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps), Native_capability);
	GENODE_RPC(Rpc_free_rpc_cap, void, free_rpc_cap, Native_capability);

	GENODE_RPC(Rpc_address_space, Capability<Region_map>, address_space);
	GENODE_RPC(Rpc_stack_area,    Capability<Region_map>, stack_area);
	GENODE_RPC(Rpc_linker_area,   Capability<Region_map>, linker_area);

	GENODE_RPC_THROW(Rpc_ref_account, void, ref_account,
	                 GENODE_TYPE_LIST(Invalid_session), Capability<Pd_session>);
	GENODE_RPC_THROW(Rpc_transfer_cap_quota, void, transfer_quota,
	                 GENODE_TYPE_LIST(Out_of_caps, Invalid_session, Undefined_ref_account),
	                 Capability<Pd_session>, Cap_quota);
	GENODE_RPC(Rpc_cap_quota, Cap_quota, cap_quota);
	GENODE_RPC(Rpc_used_caps, Cap_quota, used_caps);

	GENODE_RPC_THROW(Rpc_alloc, Ram_dataspace_capability, alloc,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Undefined_ref_account),
	                 size_t, Cache_attribute);
	GENODE_RPC(Rpc_free, void, free, Ram_dataspace_capability);
	GENODE_RPC_THROW(Rpc_transfer_ram_quota, void, transfer_quota,
	                 GENODE_TYPE_LIST(Out_of_ram, Invalid_session, Undefined_ref_account),
	                 Capability<Pd_session>, Ram_quota);
	GENODE_RPC(Rpc_ram_quota, Ram_quota, ram_quota);
	GENODE_RPC(Rpc_used_ram, Ram_quota, used_ram);

	GENODE_RPC(Rpc_native_pd, Capability<Native_pd>, native_pd);

	GENODE_RPC_INTERFACE(Rpc_assign_parent, Rpc_assign_pci, Rpc_map,
	                     Rpc_alloc_signal_source, Rpc_free_signal_source,
	                     Rpc_alloc_context, Rpc_free_context, Rpc_submit,
	                     Rpc_alloc_rpc_cap, Rpc_free_rpc_cap, Rpc_address_space,
	                     Rpc_stack_area, Rpc_linker_area, Rpc_ref_account,
	                     Rpc_transfer_cap_quota, Rpc_cap_quota, Rpc_used_caps,
	                     Rpc_alloc, Rpc_free,
	                     Rpc_transfer_ram_quota, Rpc_ram_quota, Rpc_used_ram,
	                     Rpc_native_pd);
};

#endif /* _INCLUDE__PD_SESSION__PD_SESSION_H_ */
