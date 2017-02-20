/*
 * \brief  Platform specific parts of the core CPU session
 * \author Martin Stein
 * \date   2012-03-21
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_
#define _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_

#include <base/log.h>
#include <base/allocator.h>

namespace Genode
{
	/**
	 * Thread allocator for cores CPU service
	 *
	 * Normally one would use a SLAB for threads because usually they
	 * are tiny objects, but in 'base-hw' they contain the whole kernel
	 * object in addition. Thus we use the given allocator directly.
	 */
	class Cpu_thread_allocator : public Allocator
	{
		Allocator * const _alloc;

		public:

			/**
			 * Constructor
			 *
			 * \param alloc  allocator backend
			 */
			Cpu_thread_allocator(Allocator * alloc) : _alloc(alloc) { }

			/*************************
			 ** Allocator interface **
			 *************************/

			bool alloc(size_t size, void **out_addr) override {
				return _alloc->alloc(size, out_addr); }

			void free(void *addr, size_t size) override {
				_alloc->free(addr, size); }

			size_t consumed() const override
			{
				warning(__func__, "unexpectedly called");
				while (1) ;
				return 0;
			}

			size_t overhead(size_t size) const override
			{
				warning(__func__, "unexpectedly called");
				while (1) ;
				return 0;
			}

			bool need_size_for_free() const override {
				return _alloc->need_size_for_free(); }
	};
}

#endif /* _CORE__INCLUDE__CPU_THREAD_ALLOCATOR_H_ */
