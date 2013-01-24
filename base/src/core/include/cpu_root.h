/*
 * \brief  CPU root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CPU_ROOT_H_
#define _CORE__INCLUDE__CPU_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* Core includes */
#include <cpu_session_component.h>

namespace Genode {

	class Cpu_root : public Root_component<Cpu_session_component>
	{
		private:

			Rpc_entrypoint    *_thread_ep;
			Pager_entrypoint  *_pager_ep;
			Allocator         *_md_alloc;

		protected:

			Cpu_session_component *_create_session(const char *args) {
				return new (md_alloc())
					Cpu_session_component(_thread_ep, _pager_ep, _md_alloc, args); }

			void _upgrade_session(Cpu_session_component *cpu, const char *args)
			{
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").long_value(0);
				cpu->upgrade_ram_quota(ram_quota);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep   entry point for managing cpu session objects
			 * \param thread_ep    entry point for managing threads
			 * \param md_alloc     meta data allocator to be used by root component
			 */
			Cpu_root(Rpc_entrypoint    *session_ep,
			         Rpc_entrypoint    *thread_ep,
			         Pager_entrypoint  *pager_ep,
			         Allocator         *md_alloc)
			:
				Root_component<Cpu_session_component>(session_ep, md_alloc),
				_thread_ep(thread_ep), _pager_ep(pager_ep), _md_alloc(md_alloc)
			{ }
	};
}

#endif /* _CORE__INCLUDE__CPU_ROOT_H_ */
