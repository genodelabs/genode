/*
 * \brief  Linux-specific support code for the thread API
 * \author Norman Feske
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <base/printf.h>
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
		Local_addr attach(Genode::Dataspace_capability ds_cap,
		                  Genode::size_t size, Genode::off_t offset,
		                  bool use_local_addr, Local_addr local_addr,
		                  bool executable)
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

		void detach(Local_addr local_addr) {
			PWRN("stack area detach from 0x%p - not implemented", (void *)local_addr); }

		void fault_handler(Genode::Signal_context_capability) { }

		State state() { return State(); }

		Genode::Dataspace_capability dataspace() {
			return Genode::Dataspace_capability(); }
};


class Stack_area_ram_session : public Genode::Ram_session
{
	public:

		Genode::Ram_dataspace_capability alloc(Genode::size_t size,
		                                       Genode::Cache_attribute) {
			return Genode::Ram_dataspace_capability(); }

		void free(Genode::Ram_dataspace_capability) { }

		int ref_account(Genode::Ram_session_capability) { return 0; }

		int transfer_quota(Genode::Ram_session_capability, Genode::size_t) { return 0; }

		size_t quota() { return 0; }

		size_t used() { return 0; }
};


/**
 * Return single instance of the stack-area RM and RAM session
 */
namespace Genode {

	Region_map  *env_stack_area_region_map;
	Ram_session *env_stack_area_ram_session;

	void init_stack_area()
	{
		static Stack_area_region_map rm_inst;
		env_stack_area_region_map = &rm_inst;

		static Stack_area_ram_session ram_inst;
		env_stack_area_ram_session = &ram_inst;
	}
}

