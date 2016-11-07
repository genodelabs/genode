/*
 * \brief  Paging framework
 * \author Martin Stein
 * \date   2013-11-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PAGER_H_
#define _CORE__INCLUDE__PAGER_H_

/* Genode includes */
#include <base/session_label.h>
#include <base/thread.h>
#include <base/object_pool.h>
#include <base/signal.h>
#include <pager/capability.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

/* core-local includes */
#include <kernel/signal_receiver.h>
#include <object.h>
#include <rpc_cap_factory.h>

namespace Genode
{

	/**
	 * Translation of a virtual page frame
	 */
	struct Mapping;

	/**
	 * Interface between the generic paging system and the base-hw backend
	 */
	class Ipc_pager;

	/**
	 * Represents a faulter and its paging context
	 */
	class Pager_object;

	/**
	 * Paging entry point that manages a pool of pager objects
	 */
	class Pager_entrypoint;

	enum { PAGER_EP_STACK_SIZE = sizeof(addr_t) * 2048 };
}

struct Genode::Mapping
{
	addr_t          virt_address;
	addr_t          phys_address;
	Cache_attribute cacheable;
	bool            io_mem;
	unsigned        size_log2;
	bool            writable;

	/**
	 * Constructor for invalid mappings
	 */
	Mapping();

	/**
	 * Constructor for valid mappings
	 */
	Mapping(addr_t const va, addr_t const pa, Cache_attribute const c,
	        bool const io, unsigned const sl2, bool const w);

	/**
	 * Prepare for the application of the mapping
	 */
	void prepare_map_operation();
};

class Genode::Ipc_pager
{
	protected:

		/**
		 * Page-fault data that is read from the faulters thread registers
		 */
		struct Fault_thread_regs
		{
			addr_t ip;
			addr_t addr;
			addr_t writes;
			addr_t signal;
		};

		Fault_thread_regs _fault;
		Mapping           _mapping;

	public:

		/**
		 * Instruction pointer of current page fault
		 */
		addr_t fault_ip() const;

		/**
		 * Faulter-local fault address of current page fault
		 */
		addr_t fault_addr() const;

		/**
		 * Access direction of current page fault
		 */
		bool write_fault() const;

		/**
		 * Input mapping data as reply to current page fault
		 */
		void set_reply_mapping(Mapping m);
};


class Genode::Pager_object : public Object_pool<Pager_object>::Entry,
                             public Genode::Kernel_object<Kernel::Signal_context>
{
	friend class Pager_entrypoint;

	private:

		unsigned long    const _badge;
		Cpu_session_capability _cpu_session_cap;
		Thread_capability      _thread_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param badge  user identifaction of pager object
		 */
		Pager_object(Cpu_session_capability cpu_session_cap,
		             Thread_capability thread_cap, unsigned const badge,
		             Affinity::Location, Session_label const&,
		             Cpu_session::Name const&);

		/**
		 * User identification of pager object
		 */
		unsigned long badge() const { return _badge; }

		/**
		 * Resume faulter
		 */
		void wake_up();

		/**
		 * Unnecessary as base-hw doesn't use exception handlers
		 */
		void exception_handler(Signal_context_capability);

		/**
		 * Install information that is necessary to handle page faults
		 *
		 * \param receiver  signal receiver that receives the page faults
		 */
		void start_paging(Kernel::Signal_receiver * receiver);

		/**
		 * Called when a page-fault finally could not be resolved
		 */
		void unresolved_page_fault_occurred();

		void print(Output &out) const;


		/******************
		 ** Pure virtual **
		 ******************/

		/**
		 * Request a mapping that resolves a fault directly
		 *
		 * \param p  offers the fault data and receives mapping data
		 *
		 * \retval   0  succeeded
		 * \retval !=0  fault can't be received directly
		 */
		virtual int pager(Ipc_pager & p) = 0;


		/***************
		 ** Accessors **
		 ***************/

		Cpu_session_capability cpu_session_cap() const { return _cpu_session_cap; }
		Thread_capability      thread_cap()      const { return _thread_cap; }
};


class Genode::Pager_entrypoint : public Object_pool<Pager_object>,
                                 public Thread_deprecated<PAGER_EP_STACK_SIZE>,
                                 public Kernel_object<Kernel::Signal_receiver>,
                                 public Ipc_pager
{
	public:

		/**
		 * Constructor
		 */
		Pager_entrypoint(Rpc_cap_factory &);

		/**
		 * Associate pager object 'obj' with entry point
		 */
		Pager_capability manage(Pager_object * const obj);

		/**
		 * Dissolve pager object 'obj' from entry point
		 */
		void dissolve(Pager_object * const obj);


		/**********************
		 ** Thread interface **
		 **********************/

		void entry();
};

#endif /* _CORE__INCLUDE__PAGER_H_ */
