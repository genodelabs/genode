/*
 * \brief   Platform specific part of a Genode protection domain
 * \author  Martin Stein
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
#include <tlb.h>
#include <platform.h>
#include <platform_thread.h>
#include <address_space.h>

namespace Kernel
{
	Genode::size_t pd_size();
	unsigned       pd_alignm_log2();
}

namespace Genode
{
	/**
	 * Regain all administrative memory that isn't used anymore by 'tlb'
	 */
	inline void regain_ram_from_tlb(Tlb * tlb)
	{
		size_t s;
		void * base;
		while (tlb->regain_memory(base, s)) {
			platform()->ram_alloc()->free(base, s);
		}
	}

	class Platform_thread;

	/**
	 * Platform specific part of a Genode protection domain
	 */
	class Platform_pd : public Address_space
	{
		unsigned           _id;
		Native_capability  _parent;
		Native_thread_id   _main_thread;
		char const * const _label;
		Tlb *              _tlb;

		public:

			/**
			 * Constructor
			 */
			Platform_pd(char const *label) : _main_thread(0), _label(label)
			{
				/* get some aligned space for the kernel object */
				void * kernel_pd = 0;
				Range_allocator * ram = platform()->ram_alloc();
				bool kernel_pd_ok =
					ram->alloc_aligned(Kernel::pd_size(), &kernel_pd,
					                   Kernel::pd_alignm_log2()).is_ok();
				if (!kernel_pd_ok) {
					PERR("failed to allocate kernel object");
					throw Root::Quota_exceeded();
				}
				/* create kernel object */
				_id = Kernel::new_pd(kernel_pd, this);
				if (!_id) {
					PERR("failed to create kernel object");
					throw Root::Unavailable();
				}
				_tlb = (Tlb *)kernel_pd;
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
					return t->join_pd(_id, 1, Address_space::weak_ptr());
				}
				return t->join_pd(_id, 0, Address_space::weak_ptr());
			}

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

			char const * const label() { return _label; }


			/*****************************
			 ** Address-space interface **
			 *****************************/

			void flush(addr_t, size_t) { PDBG("not implemented"); }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */

