/*
 * \brief  CPU root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

			Ram_allocator          &_ram_alloc;
			Region_map             &_local_rm;
			Rpc_entrypoint         &_thread_ep;
			Pager_entrypoint       &_pager_ep;
			Allocator              &_md_alloc;
			Trace::Source_registry &_trace_sources;

		protected:

			Cpu_session_component *_create_session(char const *args,
			                                       Affinity const &affinity) override {

				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").ulong_value(0);

				if (ram_quota < Trace::Control_area::SIZE)
					throw Insufficient_ram_quota();

				return new (md_alloc())
					Cpu_session_component(*this->ep(),
					                      session_resources_from_args(args),
					                      session_label_from_args(args),
					                      session_diag_from_args(args),
					                      _ram_alloc, _local_rm,
					                      _thread_ep, _pager_ep, _trace_sources,
					                      args, affinity, 0);
			}

			void _upgrade_session(Cpu_session_component *cpu, const char *args) override
			{
				cpu->upgrade(ram_quota_from_args(args));
				cpu->upgrade(cap_quota_from_args(args));
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep   entry point for managing cpu session objects
			 * \param thread_ep    entry point for managing threads
			 * \param md_alloc     meta data allocator to be used by root component
			 */
			Cpu_root(Ram_allocator          &ram_alloc,
			         Region_map             &local_rm,
			         Rpc_entrypoint         &session_ep,
			         Rpc_entrypoint         &thread_ep,
			         Pager_entrypoint       &pager_ep,
			         Allocator              &md_alloc,
			         Trace::Source_registry &trace_sources)
			:
				Root_component<Cpu_session_component>(&session_ep, &md_alloc),
				_ram_alloc(ram_alloc), _local_rm(local_rm),
				_thread_ep(thread_ep), _pager_ep(pager_ep),
				_md_alloc(md_alloc), _trace_sources(trace_sources)
			{ }
	};
}

#endif /* _CORE__INCLUDE__CPU_ROOT_H_ */
