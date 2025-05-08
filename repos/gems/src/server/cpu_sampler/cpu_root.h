/*
 * \brief  CPU root interface
 * \author Christian Prochaska
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CPU_ROOT_H_
#define _CPU_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* local includes */
#include "cpu_session_component.h"
#include "cpu_thread_component.h"
#include "thread_list_change_handler.h"

namespace Cpu_sampler {
	using namespace Genode;
	class Cpu_root;
}

class Cpu_sampler::Cpu_root : public Root_component<Cpu_session_component>
{
	private:

		Rpc_entrypoint             &_thread_ep;
		Env                        &_env;
		Allocator                  &_md_alloc;
		Thread_list                &_thread_list;
		Thread_list_change_handler &_thread_list_change_handler;

	protected:

		Create_result _create_session(const char *args) override
		{
			return *new (md_alloc())
				Cpu_session_component(_thread_ep, _env, _md_alloc, _thread_list,
				                      _thread_list_change_handler, args);
		}

		void _upgrade_session(Cpu_session_component &cpu, const char *args) override
		{
			cpu.upgrade_quota(args);
		}

	public:

		Cpu_root(Rpc_entrypoint             &session_ep,
		         Rpc_entrypoint             &thread_ep,
		         Env                        &env,
		         Allocator                  &md_alloc,
		         Thread_list                &thread_list,
		         Thread_list_change_handler &thread_list_change_handler)
		: Root_component<Cpu_session_component>(&session_ep, &md_alloc),
		  _thread_ep(thread_ep), _env(env),
		  _md_alloc(md_alloc),
		  _thread_list(thread_list),
		  _thread_list_change_handler(thread_list_change_handler) { }

};

#endif /* _CPU_ROOT_H_ */
