/*
 * \brief  Pistachio platform
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2007-09-10
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_H_
#define _CORE__INCLUDE__PLATFORM_H_

/* Genode includes */
#include <base/allocator_avl.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

/* core includes */
#include <synced_range_allocator.h>
#include <platform_generic.h>
#include <platform_thread.h>
#include <platform_pd.h>

namespace Core { class Platform; }


class Core::Platform : public Platform_generic
{
	private:

		/*
		 * Shortcut for the type of allocator instances for physical resources
		 */
		typedef Synced_range_allocator<Allocator_avl> Phys_allocator;

		Phys_allocator _ram_alloc;      /* RAM allocator */
		Phys_allocator _io_mem_alloc;   /* MMIO allocator */
		Phys_allocator _io_port_alloc;  /* I/O port allocator */
		Phys_allocator _irq_alloc;      /* IRQ allocator */
		Phys_allocator _region_alloc;   /* virtual memory allocator for core */
		Rom_fs         _rom_fs { };     /* ROM file system */
		Rom_module     _kip_rom;        /* ROM module for Fiasco KIP */
		addr_t         _vm_start = 0;   /* begin of virtual memory */
		size_t         _vm_size  = 0;   /* size of virtual memory */

		/*
		 * We do not export any boot module loaded before FIRST_ROM.
		 */
		enum { FIRST_ROM = 3 };

		/**
		 * Setup base resources
		 *
		 * - Map and provide KIP as ROM module
		 * - Initializes region allocator
		 */
		void _setup_basics();

		/**
		 * Setup preemption flags
		 */
		void _setup_preemption();

		/**
		 * Setup RAM, IO_MEM, and region allocators
		 */
		void _setup_mem_alloc();

		/**
		 * Setup I/O port space allocator
		 */
		void _setup_io_port_alloc();

		/**
		 * Setup IRQ allocator
		 */
		void _setup_irq_alloc();

		/**
		 * Parse multi-boot information and update ROM database
		 */
		void _init_rom_modules();

		addr_t _rom_module_phys(addr_t virt) { return virt; }

	public:

		/**
		 * Pager object representing the pager of core namely sigma0
		 */
		struct Sigma0 : public Pager_object
		{
			/**
			 * Constructor
			 */
			Sigma0();

			/* never called */
			Pager_result pager(Ipc_pager &) override { return Pager_result::STOP; }
		};

		/**
		 * Return singleton instance of Sigma0 pager object
		 */
		static Sigma0 &sigma0();

		/**
		 * Core pager thread that handles core-internal page-faults
		 */
		struct Core_pager : public Platform_thread, public Pager_object
		{
			/**
			 * Constructor
			 */
			Core_pager(Platform_pd &core_pd);

			/* never called */
			Pager_result pager(Ipc_pager &) override { return Pager_result::STOP; }
		};

		/**
		 * Return singleton instance of core pager object
		 */
		Core_pager &core_pager();

		/**
		 * Constructor
		 */
		Platform();

		/**
		 * Return singleton instance of core PD object
		 */
		Platform_pd &core_pd();


		/********************************
		 ** Generic platform interface **
		 ********************************/

		Range_allocator &core_mem_alloc() override { return _ram_alloc; }
		Range_allocator &ram_alloc()      override { return _ram_alloc; }
		Range_allocator &io_mem_alloc()   override { return _io_mem_alloc; }
		Range_allocator &io_port_alloc()  override { return _io_port_alloc; }
		Range_allocator &irq_alloc()      override { return _irq_alloc; }
		Range_allocator &region_alloc()   override { return _region_alloc; }
		addr_t           vm_start() const override { return _vm_start; }
		size_t           vm_size()  const override { return _vm_size; }
		Rom_fs          &rom_fs()         override { return _rom_fs; }
		size_t           max_caps() const override { return Capability_space::max_caps(); }

		void wait_for_exit() override;

		Affinity::Space affinity_space() const override
		{
			/*
			 * Ignore topology of CPU nodes, just return a one-dimensional
			 * affinity space.
			 */
			return Affinity::Space(L4_NumProcessors(Pistachio::get_kip()), 1);
		}
};

#endif /* _CORE__INCLUDE__PLATFORM_H_ */
