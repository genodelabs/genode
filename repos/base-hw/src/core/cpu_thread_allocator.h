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

#ifndef _CORE__CPU_THREAD_ALLOCATOR_H_
#define _CORE__CPU_THREAD_ALLOCATOR_H_

/* Genode includes */
#include <base/allocator.h>

/* core includes */
#include <assertion.h>

namespace Core { class Cpu_thread_allocator; }


/**
 * Thread allocator for cores CPU service
 *
 * Normally one would use a SLAB for threads because usually they
 * are tiny objects, but in 'base-hw' they contain the whole kernel
 * object in addition. Thus we use the given allocator directly.
 */
class Core::Cpu_thread_allocator : public Allocator
{
	private:

		/*
		 * Noncopyable
		 */
		Cpu_thread_allocator(Cpu_thread_allocator const &);
		Cpu_thread_allocator &operator = (Cpu_thread_allocator const &);

		Allocator &_alloc;

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  allocator backend
		 */
		Cpu_thread_allocator(Allocator &alloc) : _alloc(alloc) { }


		/*********************************
		 ** Memory::Allocator interface **
		 *********************************/

		Alloc_result try_alloc(size_t size) override
		{
			return _alloc.try_alloc(size).convert<Alloc_result>(
				[&] (Allocation &a) -> Alloc_result {
					a.deallocate = false; return { *this, a }; },
				[&] (Alloc_error e) { return e; });
		}

		void _free(Allocation &a) override { _alloc.free(a.ptr, a.num_bytes); }


		/****************************************
		 ** Legacy Genode::Allocator interface **
		 ****************************************/

		void free(void *addr, size_t size) override {
			_alloc.free(addr, size); }

		size_t consumed() const override { ASSERT_NEVER_CALLED; }

		size_t overhead(size_t) const override { ASSERT_NEVER_CALLED; }

		bool need_size_for_free() const override {
			return _alloc.need_size_for_free(); }
};

#endif /* _CORE__CPU_THREAD_ALLOCATOR_H_ */
