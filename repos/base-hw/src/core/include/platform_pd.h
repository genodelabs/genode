/*
 * \brief   Platform specific part of a Genode protection domain
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/printf.h>
#include <root/root.h>
#include <util/construct_at.h>

/* Core includes */
#include <translation_table.h>
#include <platform.h>
#include <platform_thread.h>
#include <address_space.h>
#include <page_slab.h>
#include <kernel/kernel.h>

namespace Hw
{
	/**
	 * Memory virtualization interface of a protection domain
	 */
	class Address_space;
}

namespace Genode
{
	class Platform_thread; /* forward declaration */

	/**
	 * Platform specific part of a Genode protection domain
	 */
	class Platform_pd;

	/**
	 * Platform specific part of Core's protection domain
	 */
	class Core_platform_pd;
}


class Hw::Address_space : public Genode::Address_space
{
	private:

		friend class Genode::Platform;
		friend class Genode::Core_mem_allocator::Mapped_mem_allocator;

		Genode::Lock                _lock;    /* table lock           */
		Genode::Translation_table * _tt;      /* table virtual addr.  */
		Genode::Translation_table * _tt_phys; /* table physical addr. */
		Genode::Page_slab         * _pslab;   /* page table allocator */
		Kernel::Pd                * _kernel_pd;

		static inline Genode::Core_mem_allocator * _cma();
		static inline void * _tt_alloc();

	protected:


		/**
		 * Core-specific constructor
		 *
		 * \param pd    pointer to kernel's pd object
		 * \param tt    pointer to translation table
		 * \param slab  pointer to page slab allocator
		 */
		Address_space(Kernel::Pd * pd,
		              Genode::Translation_table * tt,
		              Genode::Page_slab * slab);

	public:

		/**
		 * Constructor
		 *
		 * \param pd    pointer to kernel's pd object
		 */
		Address_space(Kernel::Pd* pd);

		/**
		 * Insert memory mapping into translation table of the address space
		 *
		 * \param virt   virtual start address
		 * \param phys   physical start address
		 * \param size   size of memory region
		 * \param flags  translation table flags (e.g. caching attributes)
		 */
		bool insert_translation(Genode::addr_t virt, Genode::addr_t phys,
		                        Genode::size_t size, Genode::Page_flags flags);


		/*****************************
		 ** Address-space interface **
		 *****************************/

		void flush(Genode::addr_t, Genode::size_t);


		/***************
		 ** Accessors **
		 ***************/

		Kernel::Pd * kernel_pd() { return _kernel_pd; }
		Genode::Translation_table * translation_table() { return _tt; }
		Genode::Translation_table * translation_table_phys() {
			return _tt_phys; }
};


class Genode::Platform_pd : public Hw::Address_space
{
	private:

		Native_capability   _parent;
		bool                _thread_associated = false;
		char const * const  _label;
		uint8_t             _kernel_object[sizeof(Kernel::Pd)];

	protected:

		/**
		 * Constructor for core pd
		 *
		 * \param tt    translation table address
		 * \param slab  page table allocator
		 */
		Platform_pd(Translation_table * tt, Page_slab * slab);

	public:

		/**
		 * Constructor for non-core pd
		 *
		 * \param label  name of protection domain
		 */
		Platform_pd(Allocator * md_alloc, char const *label);

		~Platform_pd();

		/**
		 * Bind thread 't' to protection domain
		 *
		 * \return  0  on success or
		 *         -1  if failed
		 */
		int bind_thread(Platform_thread * t);

		/**
		 * Unbind thread 't' from protection domain
		 */
		void unbind_thread(Platform_thread *t);


		/**
		 * Assign parent interface to protection domain
		 */
		int assign_parent(Native_capability parent);


		/***************
		 ** Accessors **
		 ***************/

		char const * const  label() { return _label; }
};


class Genode::Core_platform_pd : public Genode::Platform_pd
{
	private:

		static inline Translation_table * const _table();
		static inline Page_slab * const _slab();

		/**
		 * Establish initial one-to-one mappings for core/kernel.
		 * This function avoids to take the core-pd's translation table
		 * lock in contrast to normal translation insertions to
		 * circumvent strex/ldrex problems in early bootstrap code
		 * on some ARM SoCs.
		 *
		 * \param start   physical/virtual start address of area
		 * \param end     physical/virtual end address of area
		 * \param io_mem  true if it should be marked as device memory
		 */
		void _map(addr_t start, addr_t end, bool io_mem);

	public:

		Core_platform_pd();
};

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */

