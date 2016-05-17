/*
 * \brief  CPU root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CPU_ROOT_H_
#define _CPU_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* core includes */
#include <cpu_session_component.h>

/* GDB monitor includes */
#include "genode_child_resources.h"

namespace Gdb_monitor { class Cpu_root; }

class Gdb_monitor::Cpu_root : public Root_component<Cpu_session_component>
{
	private:

		Rpc_entrypoint          *_thread_ep;
		Allocator               *_md_alloc;
		Pd_session_capability    _core_pd;
		Genode::Signal_receiver *_signal_receiver;
		Genode_child_resources  *_genode_child_resources;

	protected:

		Cpu_session_component *_create_session(const char *args)
		{
			Cpu_session_component *cpu_session_component =
				new (md_alloc())
					Cpu_session_component(_thread_ep,
					                      _md_alloc,
					                      _core_pd,
					                      _signal_receiver,
					                      args);
			_genode_child_resources->cpu_session_component(cpu_session_component);
			return cpu_session_component;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep   entry point for managing cpu session objects
		 * \param thread_ep    entry point for managing threads
		 * \param md_alloc     meta data allocator to be used by root component
		 */
		Cpu_root(Rpc_entrypoint *session_ep,
		         Rpc_entrypoint *thread_ep,
				 Allocator *md_alloc,
				 Pd_session_capability core_pd,
				 Genode::Signal_receiver *signal_receiver,
				 Genode_child_resources *genode_child_resources)
		:
			Root_component<Cpu_session_component>(session_ep, md_alloc),
			_thread_ep(thread_ep),
			_md_alloc(md_alloc),
			_core_pd(core_pd),
			_signal_receiver(signal_receiver),
			_genode_child_resources(genode_child_resources)
		{ }
};

#endif /* _CPU_ROOT_H_ */
