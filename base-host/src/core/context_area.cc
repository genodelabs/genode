/*
 * \brief  Support code for the thread API
 * \author Norman Feske
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <base/printf.h>
#include <base/thread.h>


/**
 * Region-manager session for allocating thread contexts
 */
class Context_area_rm_session : public Genode::Rm_session
{
	public:

		/**
		 * Attach backing store to thread-context area
		 */
		Local_addr attach(Genode::Dataspace_capability ds_cap,
		                  Genode::size_t size, Genode::off_t offset,
		                  bool use_local_addr, Local_addr local_addr)
		{
			PWRN("not implemented");
			return local_addr;
		}

		void detach(Local_addr local_addr) {
			PWRN("context area detach from 0x%p - not implemented", (void *)local_addr); }

		Genode::Pager_capability add_client(Genode::Thread_capability) {
			return Genode::Pager_capability(); }

		void fault_handler(Genode::Signal_context_capability) { }

		State state() { return State(); }

		Genode::Dataspace_capability dataspace() {
			return Genode::Dataspace_capability(); }
};


class Context_area_ram_session : public Genode::Ram_session
{
	public:

		Genode::Ram_dataspace_capability alloc(Genode::size_t size, bool) {
			return Genode::Ram_dataspace_capability(); }

		void free(Genode::Ram_dataspace_capability) { }

		int ref_account(Genode::Ram_session_capability) { return 0; }

		int transfer_quota(Genode::Ram_session_capability, Genode::size_t) { return 0; }

		Genode::size_t quota() { return 0; }

		Genode::size_t used() { return 0; }
};


/**
 * Return single instance of the context-area RM and RAM session
 */
namespace Genode {

	Rm_session *env_context_area_rm_session()
	{
		static Context_area_rm_session inst;
		return &inst;
	}

	Ram_session *env_context_area_ram_session()
	{
		static Context_area_ram_session inst;
		return &inst;
	}
}

