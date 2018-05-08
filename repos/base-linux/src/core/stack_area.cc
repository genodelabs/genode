/*
 * \brief  Linux-specific support code for the thread API
 * \author Norman Feske
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <base/thread.h>

/* base-internal includes */
#include <base/internal/stack_area.h>
#include <base/internal/globals.h>


/**
 * Region-map for allocating stacks
 *
 * This class corresponds to the managed dataspace that is normally used for
 * organizing stacks within the stack. It "emulates" the sub address space by
 * adjusting the local address argument to 'attach' with the offset of the
 * stack area.
 */
class Stack_area_region_map : public Genode::Region_map
{
	public:

		Stack_area_region_map()
		{
			flush_stack_area();
			reserve_stack_area();
		}

		/**
		 * Attach backing store to stack area
		 */
		Local_addr attach(Genode::Dataspace_capability, Genode::size_t size,
		                  Genode::off_t, bool, Local_addr local_addr, bool,
		                  bool) override
		{
			using namespace Genode;

			/* convert stack-area-relative to absolute virtual address */
			addr_t addr = (addr_t)local_addr + stack_area_virtual_base();

			/* use anonymous mmap for allocating stack backing store */
			int   flags = MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE;
			int   prot  = PROT_READ | PROT_WRITE;
			void *res   = lx_mmap((void*)addr, size, prot, flags, -1, 0);

			if ((addr_t)res != addr)
				throw Region_conflict();

			return local_addr;
		}

		void detach(Local_addr local_addr) override
		{
			Genode::warning("stack area detach from ", (void*)local_addr,
			                " - not implemented");
		}

		void fault_handler(Genode::Signal_context_capability) override { }

		State state() override { return State(); }

		Genode::Dataspace_capability dataspace() override {
			return Genode::Dataspace_capability(); }
};


struct Stack_area_ram_allocator : Genode::Ram_allocator
{
	Genode::Ram_dataspace_capability alloc(Genode::size_t,
	                                       Genode::Cache_attribute) override {
		return Genode::Ram_dataspace_capability(); }

	void free(Genode::Ram_dataspace_capability) override { }

	Genode::size_t dataspace_size(Genode::Ram_dataspace_capability) const override { return 0; }
};


/**
 * Return single instance of the stack-area RM and RAM session
 */
namespace Genode {

	Region_map    *env_stack_area_region_map;
	Ram_allocator *env_stack_area_ram_allocator;

	void init_stack_area()
	{
		static Stack_area_region_map rm_inst;
		env_stack_area_region_map = &rm_inst;

		static Stack_area_ram_allocator ram_inst;
		env_stack_area_ram_allocator = &ram_inst;
	}
}

