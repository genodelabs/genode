/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__PD_H_
#define _KERNEL__PD_H_

/* Genode includes */
#include <cpu/atomic.h>

/* core includes */
#include <kernel/early_translations.h>
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <kernel/processor.h>
#include <translation_table.h>
#include <assert.h>

/* structure of the mode transition */
extern int            _mt_begin;
extern int            _mt_end;
extern int            _mt_user_entry_pic;
extern int            _mt_vm_entry_pic;
extern Genode::addr_t _mt_client_context_ptr;
extern Genode::addr_t _mt_master_context_begin;
extern Genode::addr_t _mt_master_context_end;

namespace Kernel
{
	/**
	 * Lock that enables synchronization inside the kernel
	 */
	class Lock;
}

class Kernel::Lock
{
	private:

		int volatile _locked;

		/**
		 * Finish all previously started memory transactions
		 */
		void _memory_barrier() { asm volatile ("" : : : "memory"); }

	public:

		Lock() : _locked(0) { }

		/**
		 * Request the lock
		 */
		void lock() { while (!Genode::cmpxchg(&_locked, 0, 1)); }

		/**
		 * Free the lock
		 */
		void unlock()
		{
			_memory_barrier();
			_locked = 0;
		}

		/**
		 * Provide guard semantic for this type of lock
		 */
		typedef Genode::Lock_guard<Kernel::Lock> Guard;
};

namespace Kernel
{
	/**
	 * Processor context of the kernel
	 */
	class Cpu_context;

	/**
	 * Controls the mode-transition page
	 *
	 * The mode transition page is a small memory region that is mapped by
	 * every PD to the same virtual address. It contains code that acts as a
	 * link between high privileged CPU mode (often called kernel) and low
	 * privileged CPU mode (often called userland). The mode transition
	 * control provides a simple interface to access the code from within
	 * the kernel.
	 */
	class Mode_transition_control;

	/**
	 * Return the system wide mode-transition control
	 */
	Mode_transition_control * mtc();

	/**
	 * Kernel backend of protection domains
	 */
	class Pd;

	class Pd_ids : public Id_allocator<MAX_PDS> { };
	typedef Object_pool<Pd> Pd_pool;

	Pd_ids  * pd_ids();
	Pd_pool * pd_pool();

	Lock & data_lock();
}

class Kernel::Cpu_context : Cpu::Context
{
	private:

		/**
		 * Hook for environment specific initializations
		 *
		 * \param stack_size  size of kernel stack
		 * \param table       base of transit translation table
		 */
		void _init(size_t const stack_size, addr_t const table);

	public:

		/**
		 * Constructor
		 *
		 * \param table  mode-transition table
		 */
		Cpu_context(Genode::Translation_table * const table);
};

class Kernel::Mode_transition_control
{
	friend class Pd;

	private:

		typedef Early_translations_allocator Allocator;
		typedef Early_translations_slab      Slab;
		typedef Genode::Translation_table    Table;
		typedef Genode::Cpu_state_modes      Cpu_state_modes;
		typedef Genode::Page_flags           Page_flags;

		Allocator   _allocator;
		Slab        _slab;
		Table       _table;
		Cpu_context _master;

		/**
		 * Return size of the mode transition
		 */
		static size_t _size() { return (addr_t)&_mt_end - (addr_t)&_mt_begin; }

		/**
		 * Return size of master-context space in the mode transition
		 */
		static size_t _master_context_size()
		{
			addr_t const begin = (addr_t)&_mt_master_context_begin;
			addr_t const end = (addr_t)&_mt_master_context_end;
			return end - begin;
		}

		/**
		 * Return virtual address of the user entry-code
		 */
		static addr_t _virt_user_entry()
		{
			addr_t const phys      = (addr_t)&_mt_user_entry_pic;
			addr_t const phys_base = (addr_t)&_mt_begin;
			return VIRT_BASE + (phys - phys_base);
		}

		/**
		 * Continue execution of client context
		 *
		 * \param context       targeted client processor-context
		 * \param processor_id  kernel name of targeted processor
		 * \param entry_raw     raw pointer to assembly entry-code
		 */
		void _continue_client(void * const context, unsigned const processor_id,
		                      addr_t const entry_raw)
		{
			/* override client-context pointer of the executing processor */
			addr_t const context_ptr_base = (addr_t)&_mt_client_context_ptr;
			size_t const context_ptr_offset = processor_id * sizeof(context);
			addr_t const context_ptr = context_ptr_base + context_ptr_offset;
			*(void * *)context_ptr = context;

			/* unlock kernel data */
			data_lock().unlock();

			/* call assembly code that applies the virtual-machine context */
			typedef void (* Entry)();
			Entry __attribute__((noreturn)) const entry = (Entry)entry_raw;
			entry();
		}

	public:

		enum {
			SIZE_LOG2  = Genode::Translation_table::MIN_PAGE_SIZE_LOG2,
			SIZE       = 1 << SIZE_LOG2,
			VIRT_BASE  = Processor::exception_entry,
			ALIGN_LOG2 = Genode::Translation_table::ALIGNM_LOG2,
			ALIGN      = 1 << ALIGN_LOG2,
		};

		/**
		 * Constructor
		 *
		 * \param c  CPU context for kernel mode entry
		 */
		Mode_transition_control();

		/**
		 * Map the mode transition page to a virtual address space
		 *
		 * \param tt   translation buffer of the address space
		 * \param ram  RAM donation for mapping (first try without)
		 */
		void map(Genode::Translation_table * tt,
		         Genode::Page_slab         * alloc)
		{
			try {
				addr_t const phys_base = (addr_t)&_mt_begin;
				tt->insert_translation(VIRT_BASE, phys_base, SIZE,
				                       Page_flags::mode_transition(), alloc);
			} catch(...) {
				PERR("Inserting exception vector in page table failed!"); }
		}

		/**
		 * Continue execution of userland context
		 *
		 * \param context       targeted userland context
		 * \param processor_id  kernel name of targeted processor
		 */
		void continue_user(Processor::Context * const context,
		                   unsigned const processor_id)
		{
			_continue_client(context, processor_id, _virt_user_entry());
		}

		/**
		 * Continue execution of virtual machine
		 *
		 * \param context       targeted virtual-machine context
		 * \param processor_id  kernel name of targeted processor
		 */
		void continue_vm(Cpu_state_modes * const context,
		                 unsigned const processor_id)
		{
			_continue_client(context, processor_id, (addr_t)&_mt_vm_entry_pic);
		}

} __attribute__((aligned(Mode_transition_control::ALIGN)));

class Kernel::Pd : public Object<Pd, MAX_PDS, Pd_ids, pd_ids, pd_pool>
{
	public:

		typedef Genode::Translation_table Table;

	private:

		Table       * const _table;
		Platform_pd * const _platform_pd;

		/* keep ready memory for size-aligned extra costs at construction */
		enum { EXTRA_RAM_SIZE = 2 * Table::MAX_COSTS_PER_TRANSLATION };
		char _extra_ram[EXTRA_RAM_SIZE];

	public:

		/**
		 * Constructor
		 *
		 * \param table        translation table of the PD
		 * \param platform_pd  core object of the PD
		 */
		Pd(Table * const table, Platform_pd * const platform_pd)
		: _table(table), _platform_pd(platform_pd) { }

		/**
		 * Let the CPU context 'c' join the PD
		 */
		void admit(Processor::Context * const c)
		{
			c->protection_domain(id());
			c->translation_table((addr_t)translation_table());
		}


		/***************
		 ** Accessors **
		 ***************/

		Platform_pd * platform_pd() const { return _platform_pd; }
		Table * translation_table() const { return _table; }
};

#endif /* _KERNEL__PD_H_ */
