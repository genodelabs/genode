/*
 * \brief  Child-policy helpers
 * \author Norman Feske
 * \date   2010-04-29
 *
 * \deprecated  use os/dynamic_rom_session.h instead
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__INIT__CHILD_POLICY_H_
#define _INCLUDE__INIT__CHILD_POLICY_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/service.h>
#include <rom_session/rom_session.h>

namespace Init {
	class Child_policy_provide_rom_file;
	using namespace Genode;
}


class Init::Child_policy_provide_rom_file
{
	private:

		struct Local_rom_session_component : Rpc_object<Rom_session>
		{
			Rpc_entrypoint      &ep;
			Dataspace_capability ds_cap;

			/**
			 * Constructor
			 */
			Local_rom_session_component(Rpc_entrypoint &ep,
			                            Dataspace_capability ds)
			: ep(ep), ds_cap(ds) { ep.manage(this); }

			~Local_rom_session_component() { ep.dissolve(this); }


			/***************************
			 ** ROM session interface **
			 ***************************/

			Rom_dataspace_capability dataspace() override {
				return static_cap_cast<Rom_dataspace>(ds_cap); }

			void sigh(Signal_context_capability) override { }

		} _session;

		Session_label const _module_name;

		typedef Local_service<Local_rom_session_component> Service;

		Service::Single_session_factory _session_factory { _session };
		Service                         _service { _session_factory };

	public:

		/**
		 * Constructor
		 */
		Child_policy_provide_rom_file(Session_label const &module_name,
		                              Dataspace_capability ds_cap,
		                              Rpc_entrypoint      *ep)
		:
			_session(*ep, ds_cap), _module_name(module_name)
		{ }

		Service *resolve_session_request_with_label(Service::Name const &name,
		                                            Session_label const &label)
		{
			return (name == "ROM" && label.last_element() == _module_name)
			       ? &_service : nullptr;
		}

		Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			return resolve_session_request_with_label(service_name,
			                                          label_from_args(args));
		}
};

#endif /* _INCLUDE__INIT__CHILD_POLICY_H_ */
