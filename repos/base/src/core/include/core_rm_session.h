/*
 * \brief  Core-specific region manager session
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_RM_SESSION_H_
#define _CORE__INCLUDE__CORE_RM_SESSION_H_

/* Genode includes */
#include <base/rpc_server.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode {

	/**
	 * Region manager that uses the physical dataspace
	 * addresses directly as virtual addresses.
	 */
	class Core_rm_session : public Rm_session
	{
		private:

			Rpc_entrypoint *_ds_ep;

		public:

			Core_rm_session(Rpc_entrypoint *ds_ep): _ds_ep(ds_ep) { }

			Local_addr attach(Dataspace_capability ds_cap, size_t size=0,
			                  off_t offset=0, bool use_local_addr = false,
			                  Local_addr local_addr = 0,
			                  bool executable = false)
			{
				auto lambda = [] (Dataspace_component *ds) {
					if (!ds)
						throw Invalid_dataspace();

					return (void *)ds->phys_addr();
				};
				return _ds_ep->apply(ds_cap, lambda);
			}

			void detach(Local_addr local_addr) { }

			Pager_capability add_client(Thread_capability thread) {
				return Pager_capability(); }

			void remove_client(Pager_capability) { }

			void fault_handler(Signal_context_capability handler) { }

			State state() { return State(); }

			Dataspace_capability dataspace() { return Dataspace_capability(); }
	};
}

#endif /* _CORE__INCLUDE__CORE_RM_SESSION_H_ */
