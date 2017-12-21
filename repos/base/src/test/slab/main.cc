/*
 * \brief  Slab allocator test
 * \author Norman Feske
 * \date   2015-03-31
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <base/slab.h>
#include <base/log.h>
#include <base/allocator_guard.h>
#include <timer_session/connection.h>


using Genode::size_t;
using Genode::log;
using Genode::error;


struct Array_of_slab_elements
{
	Genode::Slab      & slab;
	Genode::Allocator & alloc;

	size_t const num_elem;
	size_t const slab_size;

	void **elem;

	/**
	 * Return size of 'elem' array in bytes
	 */
	size_t _elem_array_size() const { return num_elem*sizeof(void *); }

	struct Alloc_failed { };

	Array_of_slab_elements(Genode::Slab &slab, size_t num_elem, size_t slab_size,
	                       Genode::Allocator & alloc)
	: slab(slab), alloc(alloc), num_elem(num_elem), slab_size(slab_size),
	  elem((void**) alloc.alloc(_elem_array_size()))
	{
		log(" allocate ", num_elem, " elements");
		for (size_t i = 0; i < num_elem; i++)
			if (!slab.alloc(slab_size, &elem[i])) throw Alloc_failed();
	}

	~Array_of_slab_elements()
	{
		log(" free ", num_elem, " elements");
		for (size_t i = 0; i < num_elem; i++)
			slab.free(elem[i], slab_size);

		alloc.free(elem, _elem_array_size());
	}

	private:

		/*
		 * Noncopyable
		 */
		Array_of_slab_elements(Array_of_slab_elements const &);
		Array_of_slab_elements &operator = (Array_of_slab_elements const &);
};


void Component::construct(Genode::Env & env)
{
	static Genode::Heap heap(env.ram(), env.rm());

	log("--- slab test ---");

	static Timer::Connection timer(env);

	enum { SLAB_SIZE = 16, BLOCK_SIZE = 256 };
	static Genode::Allocator_guard alloc(&heap, ~0UL);

	{
		Genode::Slab slab(SLAB_SIZE, BLOCK_SIZE, nullptr, &alloc);

		for (unsigned i = 1; i <= 10; i++) {
			log("round ", i, " ("
			    "used quota: ", alloc.consumed(), " "
			    "time: ", timer.elapsed_ms(), " ms)");

			Array_of_slab_elements array(slab, i*100000, SLAB_SIZE, heap);
			log(" allocation completed (used quota: ", alloc.consumed(), ")");
		}

		log(" finished (used quota: ", alloc.consumed(), ", "
		    "time: ", timer.elapsed_ms(), " ms)");

		/*
		 * The slab keeps two empty blocks around. For the test, we also need to
		 * take the overhead of the two block allocations at the heap into
		 * account.
		 */
		enum { HEAP_OVERHEAD = 9*sizeof(long) };
		if (alloc.consumed() > 2*(BLOCK_SIZE + HEAP_OVERHEAD)) {
			error("slab failed to release empty slab blocks");
			return;
		}
	}

	/* validate slab destruction */
	log("destructed slab (used quota: ", alloc.consumed(), ")");
	if (alloc.consumed() > 0) {
		error("slab failed to release all backing store");
		return;
	}

	{
		log("test double-free detection - error message is expected");

		Genode::Slab slab(SLAB_SIZE, BLOCK_SIZE, nullptr, &alloc);

		void *p = nullptr;
		{
			Array_of_slab_elements array(slab, 4096, SLAB_SIZE, heap);
			p = array.elem[1705];
		}
		slab.free(p, SLAB_SIZE);
		{
			Array_of_slab_elements array(slab, 4096, SLAB_SIZE, heap);
		}
	}

	log("Test done");
}
