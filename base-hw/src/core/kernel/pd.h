/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
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

/* core includes */
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <tlb.h>
#include <cpu.h>
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
	 * Controls the mode-transition page
	 *
	 * The mode transition page is a small memory region that is mapped by
	 * every PD to the same virtual address. It contains code that acts as a
	 * link between high privileged CPU mode (often called kernel) and low
	 * privileged CPU mode (often called userland). The mode transition
	 * control provides a simple interface to access the code from within
	 * the kernel.
	 */
	struct Mode_transition_control;

	/**
	 * Return the system wide mode-transition control
	 */
	static Mode_transition_control * mtc();

	/**
	 * Kernel backend of protection domains
	 */
	class Pd;

	class Pd_ids : public Id_allocator<MAX_PDS> { };
	typedef Object_pool<Pd> Pd_pool;

	Pd_ids  * pd_ids();
	Pd_pool * pd_pool();
}

class Kernel::Mode_transition_control
{
	friend class Pd;

	private:

		typedef Genode::Cpu             Cpu;
		typedef Genode::Cpu_state_modes Cpu_state_modes;
		typedef Genode::Page_flags      Page_flags;

		addr_t const _virt_user_entry;

	public:

		enum {
			SIZE_LOG2 = Tlb::MIN_PAGE_SIZE_LOG2,
			SIZE = 1 << SIZE_LOG2,
			VIRT_BASE = Cpu::EXCEPTION_ENTRY,
			VIRT_END = VIRT_BASE + SIZE,
			ALIGNM_LOG2 = SIZE_LOG2,
		};

		/**
		 * Constructor
		 *
		 * \param c  CPU context for kernel mode entry
		 */
		Mode_transition_control(Cpu::Context * const c)
		:
			_virt_user_entry(VIRT_BASE + ((addr_t)&_mt_user_entry_pic -
			                 (addr_t)&_mt_begin))
		{
			/* check if mode transition fits into aligned region */
			addr_t const mt_begin = (addr_t)&_mt_begin;
			addr_t const mt_end = (addr_t)&_mt_end;
			size_t const mt_size = mt_end - mt_begin;
			assert(mt_size <= SIZE);

			/* check if kernel context fits into the mode transition */
			addr_t const kc_begin = (addr_t)&_mt_master_context_begin;
			addr_t const kc_end = (addr_t)&_mt_master_context_end;
			size_t const kc_size = kc_end - kc_begin;
			assert(sizeof(Cpu::Context) <= kc_size);

			/* fetch kernel-mode context */
			Genode::memcpy(&_mt_master_context_begin, c, sizeof(Cpu::Context));
		}

		/**
		 * Map the mode transition page to a virtual address space
		 *
		 * \param tlb  translation buffer of the address space
		 * \param ram  RAM donation for mapping (first try without)
		 *
		 * \return  RAM-donation size that is needed to do the mapping
		 */
		size_t map(Tlb * tlb, addr_t ram = 0)
		{
			Page_flags::access_t const flags = Page_flags::mode_transition();
			addr_t const phys_base = (addr_t)&_mt_begin;
			return tlb->insert_translation(VIRT_BASE, phys_base, SIZE_LOG2,
			                               flags, (void *)ram);
		}

		/**
		 * Continue user-mode execution with CPU context 'c'
		 */
		void continue_user(Cpu::Context * const c)
		{
			_mt_client_context_ptr = (addr_t)c;
			((void(*)(void))_virt_user_entry)();
		}

		/**
		 * Continue VM execution with CPU state 's'
		 */
		void continue_vm(Cpu_state_modes * s)
		{
			_mt_client_context_ptr = (addr_t)s;
			((void(*)(void))&_mt_vm_entry_pic)();
		}
};

class Kernel::Pd : public Object<Pd, MAX_PDS, Pd_ids, pd_ids, pd_pool>
{
	private:

		typedef Genode::Cpu        Cpu;

		Tlb * const         _tlb;
		Platform_pd * const _platform_pd;

		/* keep ready memory for size-aligned extra costs at construction */
		enum { EXTRA_RAM_SIZE = 2 * Tlb::MAX_COSTS_PER_TRANSLATION };
		char _extra_ram[EXTRA_RAM_SIZE];

	public:

		/**
		 * Constructor
		 *
		 * \param tlb          translation lookaside buffer of the PD
		 * \param platform_pd  core object of the PD
		 */
		Pd(Tlb * const tlb, Platform_pd * const platform_pd)
		:
			_tlb(tlb), _platform_pd(platform_pd)
		{
			/* try to add translation for mode transition region */
			unsigned const slog2 = mtc()->map(tlb);

			/* extra ram needed to translate mode transition region */
			if (slog2)
			{
				/* get size aligned extra ram */
				addr_t const ram = (addr_t)&_extra_ram;
				addr_t const ram_end = ram + sizeof(_extra_ram);
				addr_t const aligned_ram = (ram_end - (1 << slog2)) &
				                           ~((1 << slog2) - 1);
				addr_t const aligned_ram_end = aligned_ram + (1 << slog2);

				/* check attributes of aligned extra ram */
				assert(aligned_ram >= ram && aligned_ram_end <= ram_end)

				/* translate mode transition region globally */
				mtc()->map(tlb, aligned_ram);
			}
		}

		/**
		 * Destructor
		 */
		~Pd() { }

		/**
		 * Let the CPU context 'c' join the PD
		 */
		void admit(Cpu::Context * const c)
		{
			c->protection_domain(id());
			c->tlb(tlb()->base());
		}


		/***************
		 ** Accessors **
		 ***************/

		Tlb * tlb() const { return _tlb; }

		Platform_pd * platform_pd() const { return _platform_pd; }
};

#endif /* _KERNEL__PD_H_ */
