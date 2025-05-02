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

#include <util/attempt.h>
#include <base/affinity.h>
#include <cpu/cpu_state.h>
#include <session/session.h>
#include <region_map/region_map.h>
#include <base/ram_allocator.h>
#include <base/local.h>

namespace Genode {
	struct Pd_account;
	struct Pd_session;
	struct Pd_ram_allocator;
	struct Pd_local_rm;
	struct Pd_session_client;
	struct Parent;
	struct Signal_context;
}


struct Genode::Pd_account : Interface, Noncopyable
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "PD"; }

	enum class Transfer_result { OK, EXCEEDED, INVALID };

	virtual Transfer_result transfer_quota(Capability<Pd_account>, Cap_quota) = 0;
	virtual Transfer_result transfer_quota(Capability<Pd_account>, Ram_quota) = 0;

	GENODE_RPC(Rpc_transfer_cap_quota, Transfer_result, transfer_quota,
	           Capability<Pd_account>, Cap_quota);
	GENODE_RPC(Rpc_transfer_ram_quota, Transfer_result, transfer_quota,
	           Capability<Pd_account>, Ram_quota);
	GENODE_RPC_INTERFACE(Rpc_transfer_ram_quota, Rpc_transfer_cap_quota);
};


struct Genode::Pd_session : Session, Pd_account
{
	/*
	 * A PD session consumes a dataspace capability for the session-object
	 * allocation, a capability for the 'Native_pd' RPC interface, its
	 * session capability, and the RPC capabilities for the 3 contained
	 * region maps.
	 *
	 * Furthermore, we account for the dataspace capabilities allocated during
	 * the component bootstrapping.
	 */
	static constexpr unsigned CAP_QUOTA = 6 + 7;
	static constexpr size_t   RAM_QUOTA = 24*1024*sizeof(long);

	using Client = Pd_session_client;

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

	struct Virt_range { addr_t start; size_t num_bytes; };

	enum class Map_result { OK, OUT_OF_RAM, OUT_OF_CAPS };

	/**
	 * Trigger eager population of page table within specified virtual range
	 *
	 * If the used kernel don't support this feature, the operation will
	 * silently ignore the request.
	 */
	virtual Map_result map(Virt_range) = 0;


	/******************************
	 ** RAM dataspace allocation **
	 ******************************/

	enum class Alloc_ram_error { OUT_OF_RAM, OUT_OF_CAPS, DENIED };

	using Alloc_ram_result = Attempt<Ram_dataspace_capability, Alloc_ram_error>;

	/**
	 * Allocate RAM dataspace
	 *
	 * \param  size   size of RAM dataspace
	 * \param  cache  selects cacheability attributes of the memory,
	 *                uncached memory, i.e., for DMA buffers
	 *
	 * \return capability to RAM dataspace, or error code of type 'Alloc_error'
	 */
	virtual Alloc_ram_result alloc_ram(size_t size, Cache cache = CACHED) = 0;

	/**
	 * Free RAM dataspace
	 *
	 * \param ds  dataspace capability as returned by alloc_ram
	 */
	virtual void free_ram(Ram_dataspace_capability ds) = 0;

	/**
	 * Return size of dataspace in bytes
	 */
	virtual size_t ram_size(Ram_dataspace_capability) = 0;


	/********************************
	 ** Support for the signal API **
	 ********************************/

	enum class Signal_source_error { OUT_OF_RAM, OUT_OF_CAPS };
	using Signal_source_result = Attempt<Capability<Signal_source>, Signal_source_error>;

	/**
	 * Return signal source for the PD
	 *
	 * The signal source provides an interface to wait for incoming signals.
	 */
	virtual Signal_source_result signal_source() = 0;

	/**
	 * Free a signal source
	 *
	 * \param cap  capability of the signal source to destroy
	 */
	virtual void free_signal_source(Capability<Signal_source> cap) = 0;

	enum class Alloc_context_error { OUT_OF_RAM, OUT_OF_CAPS, INVALID_SIGNAL_SOURCE };
	using Alloc_context_result = Attempt<Capability<Signal_context>, Alloc_context_error>;

	struct Imprint { addr_t value; };

	/**
	 * Allocate signal context
	 *
	 * \param source  signal source that shall provide the new context
	 *
	 * \param imprint  opaque value that gets delivered with signals
	 *                 originating from the allocated signal-context capability
	 * \return new signal-context capability
	 */
	virtual Alloc_context_result alloc_context(Capability<Signal_source> source,
	                                           Imprint imprint) = 0;

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

	using Alloc_rpc_cap_result = Attempt<Native_capability, Alloc_error>;

	/**
	 * Allocate new RPC-object capability
	 *
	 * \param ep  entry point that will use this capability
	 *
	 * \return new RPC capability
	 */
	virtual Alloc_rpc_cap_result alloc_rpc_cap(Native_capability ep) = 0;

	/**
	 * Free RPC-object capability
	 *
	 * \param cap  capability to free
	 */
	virtual void free_rpc_cap(Native_capability cap) = 0;


	/**************************************
	 ** Virtual address-space management **
	 **************************************/

	enum { LINKER_AREA_SIZE = 320*1024*1024UL };

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

	enum class Ref_account_result { OK, INVALID_SESSION };

	/**
	 * Define reference account for the PD session
	 */
	virtual Ref_account_result ref_account(Capability<Pd_account>) = 0;

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
	struct Native_pd;

	/**
	 * Return capability to kernel-specific PD operations
	 */
	virtual Capability<Native_pd> native_pd() = 0;


	/*******************************************
	 ** Access to system management interface **
	 *******************************************/

	struct System_control : Interface
	{
		using System_control_state = Cpu_state;

		virtual System_control_state system_control(System_control_state const &) = 0;

		GENODE_RPC(Rpc_system_control, System_control_state, system_control,
		           System_control_state const &);

		GENODE_RPC_INTERFACE(Rpc_system_control);
	};


	using Managing_system_state = Cpu_state;

	/**
	 * Call privileged system control functionality of kernel or firmware
	 */

	virtual Capability<System_control> system_control_cap(Affinity::Location const) = 0;


	/*******************************************
	 ** Support for user-level device drivers **
	 *******************************************/

	/**
	 * Return start address of the dataspace to be used for DMA transfers
	 *
	 * The intended use of this function is the use of RAM dataspaces as DMA
	 * buffers. On systems without IOMMU, device drivers need to know the
	 * physical address of DMA buffers for issuing DMA transfers.
	 *
	 * \return  DMA address, or 0 if the dataspace is invalid or the
	 *          PD lacks the permission to obtain the information
	 */
	virtual addr_t dma_addr(Ram_dataspace_capability) = 0;

	enum class Attach_dma_error { OUT_OF_RAM, OUT_OF_CAPS, DENIED };

	using Attach_dma_result = Attempt<Ok, Attach_dma_error>;

	/**
	 * Attach dataspace to I/O page table at specified address 'at'
	 *
	 * This operation is preserved to privileged system-management components
	 * like the platform driver to assign DMA buffers to device protection
	 * domains. The attach can be reverted by using 'address_space().detach()'.
	 */
	virtual Attach_dma_result attach_dma(Dataspace_capability, addr_t at) = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_assign_parent, void, assign_parent, Capability<Parent>);
	GENODE_RPC(Rpc_assign_pci,    bool, assign_pci,    addr_t, uint16_t);
	GENODE_RPC(Rpc_map, Map_result, map, Virt_range);
	GENODE_RPC(Rpc_signal_source, Signal_source_result, signal_source);
	GENODE_RPC(Rpc_free_signal_source, void, free_signal_source, Capability<Signal_source>);
	GENODE_RPC(Rpc_alloc_context, Alloc_context_result, alloc_context,
	           Capability<Signal_source>, Imprint);
	GENODE_RPC(Rpc_free_context, void, free_context, Capability<Signal_context>);
	GENODE_RPC(Rpc_submit, void, submit, Capability<Signal_context>, unsigned);
	GENODE_RPC(Rpc_alloc_rpc_cap, Alloc_rpc_cap_result, alloc_rpc_cap, Native_capability);
	GENODE_RPC(Rpc_free_rpc_cap, void, free_rpc_cap, Native_capability);
	GENODE_RPC(Rpc_address_space, Capability<Region_map>, address_space);
	GENODE_RPC(Rpc_stack_area,    Capability<Region_map>, stack_area);
	GENODE_RPC(Rpc_linker_area,   Capability<Region_map>, linker_area);
	GENODE_RPC(Rpc_ref_account, Ref_account_result, ref_account, Capability<Pd_account>);
	GENODE_RPC(Rpc_cap_quota, Cap_quota, cap_quota);
	GENODE_RPC(Rpc_used_caps, Cap_quota, used_caps);
	GENODE_RPC(Rpc_alloc_ram, Alloc_ram_result, alloc_ram, size_t, Cache);
	GENODE_RPC(Rpc_free_ram, void, free_ram, Ram_dataspace_capability);
	GENODE_RPC(Rpc_ram_size, size_t, ram_size, Ram_dataspace_capability);
	GENODE_RPC(Rpc_ram_quota, Ram_quota, ram_quota);
	GENODE_RPC(Rpc_used_ram, Ram_quota, used_ram);
	GENODE_RPC(Rpc_native_pd, Capability<Native_pd>, native_pd);
	GENODE_RPC(Rpc_system_control_cap, Capability<System_control>,
	           system_control_cap, Affinity::Location);
	GENODE_RPC(Rpc_dma_addr, addr_t, dma_addr, Ram_dataspace_capability);
	GENODE_RPC(Rpc_attach_dma, Attach_dma_result, attach_dma,
	           Dataspace_capability, addr_t);

	GENODE_RPC_INTERFACE_INHERIT(Pd_account,
		Rpc_assign_parent, Rpc_assign_pci, Rpc_map,
		Rpc_signal_source, Rpc_free_signal_source,
		Rpc_alloc_context, Rpc_free_context, Rpc_submit,
		Rpc_alloc_rpc_cap, Rpc_free_rpc_cap, Rpc_address_space,
		Rpc_stack_area, Rpc_linker_area, Rpc_ref_account,
		Rpc_alloc_ram, Rpc_free_ram, Rpc_ram_size,
		Rpc_cap_quota, Rpc_used_caps, Rpc_ram_quota, Rpc_used_ram,
		Rpc_native_pd, Rpc_system_control_cap,
		Rpc_dma_addr, Rpc_attach_dma);
};


struct Genode::Pd_ram_allocator : Ram_allocator
{
	Pd_session &_pd;

	using Pd_error = Pd_session::Alloc_ram_error;

	static Ram::Error ram_error(Pd_error e)
	{
		switch (e) {
		case Pd_error::OUT_OF_CAPS: return Ram::Error::OUT_OF_CAPS;
		case Pd_error::OUT_OF_RAM:  return Ram::Error::OUT_OF_RAM;;
		case Pd_error::DENIED:      break;
		}
		return Ram::Error::DENIED;
	}

	Result try_alloc(size_t size, Cache cache) override
	{
		using Capability = Ram::Capability;

		return _pd.alloc_ram(size, cache).convert<Alloc_result>(
			[&] (Capability ds) -> Result { return { *this, { ds, size } }; },
			[&] (Pd_error e)    -> Result { return ram_error(e); });
	}

	void _free(Ram::Allocation &allocation) override
	{
		_pd.free_ram(allocation.cap);
	}

	Pd_ram_allocator(Pd_session &pd) : _pd(pd) { }
};


struct Genode::Pd_local_rm : Genode::Local::Constrained_region_map
{
	Region_map &_rm;

	Pd_local_rm(Region_map &rm) : _rm(rm) { }

	Result attach(Capability<Dataspace> ds, Attach_attr const &attr) override
	{
		if (!ds.valid())
			return Error::INVALID_DATASPACE;

		return _rm.attach(ds, attr).convert<Result>(
			[&] (Region_map::Range const &r) -> Result {
				return { *this, { (void *)r.start, r.num_bytes } }; },
			[&] (Error e) { return e; });
	}

	void _free(Attachment &a) override { _rm.detach(addr_t(a.ptr)); }
};

#endif /* _INCLUDE__PD_SESSION__PD_SESSION_H_ */
