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

/* Core includes */
#include <translation_table.h>
#include <platform.h>
#include <platform_thread.h>
#include <address_space.h>
#include <page_slab.h>
#include <kernel/kernel.h>

namespace Genode
{
	class Platform_thread;

	/**
	 * Platform specific part of a Genode protection domain
	 */
	class Platform_pd : public Address_space
	{
		protected:

			Lock                _lock; /* safeguard translation table and slab */
			unsigned            _id;
			Native_capability   _parent;
			Native_thread_id    _main_thread;
			char const * const  _label;
			Translation_table * _tt;      /* translation table virtual addr.  */
			Translation_table * _tt_phys; /* translation table physical addr. */
			uint8_t             _kernel_pd[sizeof(Kernel::Pd)];
			Page_slab         * _pslab;   /* page table allocator */

		public:

			/**
			 * Constructor for core pd
			 *
			 * \param tt    translation table address
			 * \param slab  page table allocator
			 */
			Platform_pd(Translation_table * tt, Page_slab * slab)
			: _main_thread(0), _label("core"), _tt(tt),
			  _tt_phys(tt), _pslab(slab) { }

			/**
			 * Constructor for non-core pd
			 *
			 * \param label  name of protection domain
			 */
			Platform_pd(char const *label) : _main_thread(0), _label(label)
			{
				Lock::Guard guard(_lock);

				Core_mem_allocator * cma =
					static_cast<Core_mem_allocator*>(platform()->core_mem_alloc());
				void *tt;

				/* get some aligned space for the translation table */
				if (!cma->alloc_aligned(sizeof(Translation_table), (void**)&tt,
				                        Translation_table::ALIGNM_LOG2).is_ok()) {
					PERR("failed to allocate kernel object");
					throw Root::Quota_exceeded();
				}

				_tt      = new (tt) Translation_table();
				_tt_phys = (Translation_table*) cma->phys_addr(_tt);
				_pslab   = new (cma) Page_slab(cma);
				Kernel::mtc()->map(_tt, _pslab);

				/* create kernel object */
				_id = Kernel::new_pd(&_kernel_pd, this);
				if (!_id) {
					PERR("failed to create kernel object");
					throw Root::Unavailable();
				}
			}

			/**
			 * Destructor
			 */
			~Platform_pd();

			/**
			 * Bind thread 't' to protection domain
			 *
			 * \return  0  on success or
			 *         -1  if failed
			 */
			int bind_thread(Platform_thread * t)
			{
				/* is this the first and therefore main thread in this PD? */
				if (!_main_thread)
				{
					/* annotate that we've got a main thread from now on */
					_main_thread = t->id();
					return t->join_pd(this, 1, Address_space::weak_ptr());
				}
				return t->join_pd(this, 0, Address_space::weak_ptr());
			}


			/**
			 * Unbind thread 't' from protection domain
			 */
			void unbind_thread(Platform_thread *t) {
				t->join_pd(nullptr, false, Address_space::weak_ptr()); }


			/**
			 * Assign parent interface to protection domain
			 */
			int assign_parent(Native_capability parent)
			{
				if (!parent.valid()) {
					PERR("parent invalid");
					return -1;
				}
				_parent = parent;
				return 0;
			}


			/***************
			 ** Accessors **
			 ***************/

			Lock              * lock()                      { return &_lock;   }
			unsigned const      id()                        { return _id;      }
			char const * const  label()                     { return _label;   }
			Page_slab         * page_slab()                 { return _pslab;   }
			Translation_table * translation_table()         { return _tt;      }
			Translation_table * translation_table_phys()    { return _tt_phys; }
			void                page_slab(Page_slab *pslab) { _pslab = pslab;  }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			void flush(addr_t, size_t) { PDBG("not implemented"); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */

