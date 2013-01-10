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
#include <rm_session/connection.h>
#include <pd_session/connection.h>
#include <cpu_session/client.h>
#include <parent/capability.h>

namespace Genode {

	class Process
	{
		private:

			Pd_connection      _pd;
			Thread_capability  _thread0_cap;
			Cpu_session_client _cpu_session_client;
			Rm_session_client  _rm_session_client;

			static Dataspace_capability _dynamic_linker_cap;

			/*
			 * Hook for passing additional platform-specific session
			 * arguments to the PD session. For example, on Linux a new
			 * process is created locally via 'fork' and the new PID gets
			 * then communicated to core via a PD-session argument.
			 */
			enum { PRIV_ARGBUF_LEN = 32 };
			char _priv_pd_argbuf[PRIV_ARGBUF_LEN];
			const char *_priv_pd_args(Parent_capability parent_cap,
			                          Dataspace_capability elf_data_ds,
			                          const char *name, char *const argv[]);

		public:

			/**
			 * Constructor
			 *
			 * \param elf_data_ds   dataspace that contains the elf binary
			 * \param ram_session   RAM session providing the BSS for the
			 *                      new protection domain
			 * \param cpu_session   CPU session for the new protection domain
			 * \param rm_session    RM session for the new protection domain
			 * \param parent        parent of the new protection domain
			 * \param name          name of protection domain (can be used
			 *                      for debugging)
			 * \param pd_args       platform-specific arguments supplied to
			 *                      the PD session of the process
			 *
			 * The dataspace 'elf_data_ds' can be read-only.
			 *
			 * On construction of a protection domain, execution of the initial
			 * thread is started immediately.
			 */
			Process(Dataspace_capability    elf_data_ds,
			        Ram_session_capability  ram_session,
			        Cpu_session_capability  cpu_session,
			        Rm_session_capability   rm_session,
			        Parent_capability       parent,
			        char const             *name,
			        Native_pd_args const   *args = 0);

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

			Pd_session_capability pd_session_cap() const { return _pd.cap(); }

			Thread_capability main_thread_cap() const { return _thread0_cap; }
	};
}

#endif /* _INCLUDE__BASE__PROCESS_H_ */
