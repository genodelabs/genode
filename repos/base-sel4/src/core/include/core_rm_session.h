/*
 * \brief  Core-local RM session
 * \author Norman Feske
 * \date   2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_RM_SESSION_H_
#define _CORE__INCLUDE__CORE_RM_SESSION_H_

/* Genode includes */
#include <base/printf.h>
#include <rm_session/rm_session.h>

/* core includes */
#include <dataspace_component.h>

namespace Genode { class Core_rm_session; }


class Genode::Core_rm_session : public Rm_session
{
	private:

		Rpc_entrypoint *_ds_ep;

	public:

		Core_rm_session(Rpc_entrypoint *ds_ep): _ds_ep(ds_ep) { }

		Local_addr attach(Dataspace_capability ds_cap, size_t size = 0,
		                  off_t offset = 0, bool use_local_addr = false,
		                  Local_addr local_addr = 0,
		                  bool executable = false) override;

		void detach(Local_addr) override { }

		Pager_capability add_client(Thread_capability) override {
			return Pager_capability(); }

		void remove_client(Pager_capability) override { }

		void fault_handler(Signal_context_capability) override { }

		State state() override { return State(); }

		Dataspace_capability dataspace() override { return Dataspace_capability(); }
};

#endif /* _CORE__INCLUDE__CORE_RM_SESSION_H_ */
