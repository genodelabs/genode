/*
 * \brief  Process-creation interface
 * \author Norman Feske
 * \date   2006-06-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PROCESS_H_
#define _INCLUDE__BASE__PROCESS_H_

#include <ram_session/capability.h>
#include <rm_session/client.h>
#include <pd_session/client.h>
#include <cpu_session/client.h>
#include <parent/capability.h>

namespace Genode { class Process; }


class Genode::Process
{
	private:

		Pd_session_client  _pd_session_client;
		Thread_capability  _thread0_cap;
		Cpu_session_client _cpu_session_client;
		Rm_session_client  _rm_session_client;

		static Dataspace_capability _dynamic_linker_cap;

	public:

		/**
		 * Constructor
		 *
		 * \param elf_data_ds   dataspace that contains the elf binary
		 * \param pd_session    the new protection domain
		 * \param ram_session   RAM session providing the BSS for the
		 *                      new protection domain
		 * \param cpu_session   CPU session for the new protection domain
		 * \param rm_session    RM session for the new protection domain
		 * \param parent        parent of the new protection domain
		 * \param name          name of protection domain (can be used
         *                      for debugging)
		 *
		 * The dataspace 'elf_data_ds' can be read-only.
		 *
		 * On construction of a protection domain, execution of the initial
		 * thread is started immediately.
		 */
		Process(Dataspace_capability    elf_data_ds,
		        Pd_session_capability   pd_session,
		        Ram_session_capability  ram_session,
		        Cpu_session_capability  cpu_session,
		        Rm_session_capability   rm_session,
		        Parent_capability       parent,
		        char const             *name);

		/**
		 * Destructor
		 *
		 * When called, the protection domain gets killed.
		 */
		~Process();

		static void dynamic_linker(Dataspace_capability dynamic_linker_cap)
		{
			_dynamic_linker_cap = dynamic_linker_cap;
		}

		Thread_capability main_thread_cap() const { return _thread0_cap; }
};

#endif /* _INCLUDE__BASE__PROCESS_H_ */
