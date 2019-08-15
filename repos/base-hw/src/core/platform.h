/*
 * \brief  Platform interface
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2011-12-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__PLATFORM_H_
#define _CORE__PLATFORM_H_

/* Genode includes */
#include <base/synced_allocator.h>
#include <base/allocator_avl.h>
#include <irq_session/irq_session.h>
#include <util/xml_generator.h>

/* base-hw includes */
#include <hw/boot_info.h>
#include <hw/memory_region.h>
#include <kernel/configuration.h>
#include <kernel/core_interface.h>
#include <kernel/pd.h>

/* core includes */
#include <platform_generic.h>
#include <core_region_map.h>
#include <core_mem_alloc.h>
#include <translation_table.h>
#include <assertion.h>
#include <board.h>

namespace Genode {
	class Address_space;
	class Platform;
};

class Genode::Platform : public Genode::Platform_generic
{
	private:

		Core_mem_allocator _core_mem_alloc { }; /* core-accessible memory */
		Phys_allocator     _io_mem_alloc;       /* MMIO allocator         */
		Phys_allocator     _io_port_alloc;      /* I/O port allocator     */
		Phys_allocator     _irq_alloc;          /* IRQ allocator          */
		Rom_fs             _rom_fs         { }; /* ROM file system        */

		static Hw::Boot_info<Board::Boot_info> const &_boot_info();
		static Hw::Memory_region_array const & _core_virt_regions();

		/**
		 * Initialize I/O port allocator
		 */
		void _init_io_port_alloc();

		/**
		 * Initialize IO memory allocator
		 *
		 * Use byte granularity for MMIO regions because on some platforms,
		 * devices driven by core share a physical page with devices
		 * driven outside of core. Using byte granularity allows handing
		 * out the MMIO page to trusted user-level device drivers.
		 */
		void _init_io_mem_alloc();

		/**
		 * Initialize platform_info ROM module
		 */
		void _init_platform_info();

		 /**
		  * Add additional platform-specific information.
		  */
		void _init_additional_platform_info(Genode::Xml_generator &);

		void _init_rom_modules();

		addr_t _rom_module_phys(addr_t virt);

	public:

		Platform();

		static addr_t mmio_to_virt(addr_t mmio);

		/**
		 * Return platform IRQ-number for user IRQ-number 'user_irq'
		 */
		static long irq(long const user_irq);

		/**
		 * Get MSI-related parameters from device PCI config space
		 *
		 * \param mmconf      PCI config space address of device
		 * \param address     MSI address register value to use
		 * \param data        MSI data register value to use
		 * \param irq_number  IRQ to use
		 *
		 * \return  true if the device is MSI-capable, false if not
		 */
		static bool get_msi_params(const addr_t mmconf,
		                           addr_t &address, addr_t &data,
		                           unsigned &irq_number);

		static addr_t core_phys_addr(addr_t virt);

		static addr_t                      core_page_table();
		static Hw::Page_table::Allocator & core_page_table_allocator();


		/********************************
		 ** Platform_generic interface **
		 ********************************/

		Range_allocator &core_mem_alloc() override { return _core_mem_alloc; }
		Range_allocator &ram_alloc()      override { return _core_mem_alloc.phys_alloc(); }
		Range_allocator &region_alloc()   override { return _core_mem_alloc.virt_alloc(); }
		Range_allocator &io_mem_alloc()   override { return _io_mem_alloc; }
		Range_allocator &io_port_alloc()  override { return _io_port_alloc; }
		Range_allocator &irq_alloc()      override { return _irq_alloc; }
		addr_t           vm_start() const override { return Hw::Mm::user().base; }
		size_t           vm_size()  const override { return Hw::Mm::user().size; }
		Rom_fs          &rom_fs()         override { return _rom_fs; }

		void wait_for_exit() override {
			while (1) { Kernel::stop_thread(); } };

		bool supports_direct_unmap() const override { return true; }
		Address_space &core_pd() { ASSERT_NEVER_CALLED; }

		Affinity::Space affinity_space() const override {
			return Affinity::Space(_boot_info().cpus); }

		/*
		 * The system-wide maximum number of capabilities is constrained
		 * by core's local capability space.
		 */
		size_t max_caps() const override { return Kernel::Pd::max_cap_ids; }
};

#endif /* _CORE__PLATFORM_H_ */
