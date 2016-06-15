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

/* Core includes */
#include <translation_table.h>
#include <platform.h>
#include <address_space.h>
#include <translation_table_allocator.h>
#include <object.h>
#include <kernel/configuration.h>
#include <kernel/object.h>
#include <kernel/pd.h>

namespace Hw
{
	using namespace Kernel;
	using namespace Genode;

	/**
	 * Memory virtualization interface of a protection domain
	 */
	class Address_space;
}

namespace Genode
{
	class Platform_thread; /* forward declaration */

	class Cap_space;

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
		friend class Genode::Mapped_mem_allocator;

		using Table_allocator =
			Translation_table_allocator_tpl<DEFAULT_TRANSLATION_TABLE_MAX>;

		Genode::Lock                          _lock;     /* table lock      */
		Genode::Translation_table           * _tt;       /* table virt addr */
		Genode::Translation_table           * _tt_phys;  /* table phys addr */
		Genode::Translation_table_allocator * _tt_alloc; /* table allocator */
		Kernel::Pd                          * _kernel_pd;

		static inline Genode::Core_mem_allocator * _cma();
		static inline void * _table_alloc();

	protected:


		/**
		 * Core-specific constructor
		 *
		 * \param pd        pointer to kernel's pd object
		 * \param tt        pointer to translation table
		 * \param tt_alloc  pointer to translation table allocator
		 */
		Address_space(Kernel::Pd                          * pd,
		              Genode::Translation_table           * tt,
		              Genode::Translation_table_allocator * tt_alloc);

	public:

		/**
		 * Constructor
		 *
		 * \param pd    pointer to kernel's pd object
		 */
		Address_space(Kernel::Pd* pd);

		~Address_space();

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


class Genode::Cap_space
{
	private:

		enum { SLAB_SIZE = 2 * get_page_size() };

		using Cap_slab = Tslab<Kernel::Object_identity_reference,
		                       SLAB_SIZE>;

		uint8_t    _initial_sb[SLAB_SIZE];
		Cap_slab   _slab;

	public:

		Cap_space();

		Cap_slab & capability_slab() { return _slab; }

		void upgrade_slab(Allocator &alloc);
};


class Genode::Platform_pd : public Hw::Address_space,
                            public Genode::Cap_space,
                            public Kernel_object<Kernel::Pd>
{
	private:

		Native_capability  _parent;
		bool               _thread_associated = false;
		char const * const _label;

	protected:

		/**
		 * Constructor for core pd
		 *
		 * \param tt        translation table address
		 * \param tt_alloc  translation table allocator
		 */
		Platform_pd(Translation_table           * tt,
		            Translation_table_allocator * tt_alloc);

	public:

		/**
		 * Constructor for non-core pd
		 *
		 * \param label  name of protection domain
		 */
		Platform_pd(Allocator * md_alloc, char const *label);

		/**
		 * Destructor
		 */
		~Platform_pd();

		/**
		 * Bind thread 't' to protection domain
		 */
		bool bind_thread(Platform_thread * t);

		/**
		 * Unbind thread 't' from protection domain
		 */
		void unbind_thread(Platform_thread *t);


		/**
		 * Assign parent interface to protection domain
		 */
		void assign_parent(Native_capability parent);


		/***************
		 ** Accessors **
		 ***************/

		char const * const  label()  { return _label;  }
		Native_capability   parent() { return _parent; }
};


class Genode::Core_platform_pd : public Genode::Platform_pd
{
	private:

		static inline Translation_table           * const _table();
		static inline Translation_table_allocator * const _table_alloc();

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
