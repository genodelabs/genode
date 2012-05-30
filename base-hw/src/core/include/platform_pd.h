/*
 * \brief   Platform specific part of a Genode protection domain
 * \author  Martin Stein
 * \date    2012-02-12
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_PD_H_
#define _CORE__INCLUDE__PLATFORM_PD_H_

/* Genode includes */
#include <base/printf.h>

/* Core includes */
#include <platform.h>
#include <platform_thread.h>

namespace Kernel
{
	Genode::size_t pd_size();
	unsigned       pd_alignm_log2();
}

namespace Genode
{
	class Platform_thread;

	/**
	 * Platform specific part of a Genode protection domain
	 */
	class Platform_pd
	{
		unsigned long     _id; /* ID of our kernel object */
		Native_capability _parent; /* our parent interface */
		Native_thread_id  _main_thread; /* the first thread that gets
		                                 * executed in this PD */

		public:

			/**
			 * Constructor
			 */
			Platform_pd() : _main_thread(0)
			{
				/* get some aligned space for the kernel object */
				void * kernel_pd;
				Range_allocator * ram = platform()->ram_alloc();
				assert(ram->alloc_aligned(Kernel::pd_size(), &kernel_pd,
				                          Kernel::pd_alignm_log2()));

				/* create kernel object */
				_id = Kernel::new_pd(kernel_pd);
				assert(_id);
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
					return t->join_pd(_id, 1);
				}
				return t->join_pd(_id, 0);
			}

			/**
			 * Unbind thread from protection domain
			 *
			 * Free the thread's slot and update thread object.
			 */
			void unbind_thread(Platform_thread * t);

			/**
			 * Assign parent interface to protection domain
			 */
			int assign_parent(Native_capability parent)
			{
				assert(parent.valid());
				_parent = parent;
				return 0;
			}
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_PD_H_ */

