/*
 * \brief  RM service provided to Noux processes
 * \author Norman Feske
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__LOCAL_RM_SERVICE_H_
#define _NOUX__LOCAL_RM_SERVICE_H_

/* Genode includes */
#include <base/service.h>

/* Noux includes */
#include <dataspace_registry.h>
#include <rm_session_component.h>

namespace Noux {

	using namespace Genode;


	class Rm_dataspace_info : public Dataspace_info
	{
		private:

			struct Dummy_destroyer : Dataspace_destroyer
			{
				void destroy(Dataspace_capability) { }
			} _dummy_destroyer;

			Rm_session_component &_sub_rm;

		public:

			Rm_dataspace_info(Rm_session_component &sub_rm)
			:
				Dataspace_info(sub_rm.dataspace(), _dummy_destroyer),
				_sub_rm(sub_rm)
			{ }

			Dataspace_capability fork(Ram_session_capability ram,
			                          Dataspace_destroyer &,
			                          Dataspace_registry  &ds_registry,
			                          Rpc_entrypoint      &ep)
			{
				Rm_session_component *new_sub_rm =
					new Rm_session_component(ds_registry, 0, size());

				Rm_session_capability rm_cap = ep.manage(new_sub_rm);

				/*
				 * XXX  Where to dissolve the RM session?
				 */

				_sub_rm.replay(ram, rm_cap, ds_registry, ep);

				ds_registry.insert(new Rm_dataspace_info(*new_sub_rm));

				return new_sub_rm->dataspace();
			}

			void poke(addr_t dst_offset, void const *src, size_t len)
			{
				if ((dst_offset >= size()) || (dst_offset + len > size())) {
					PERR("illegal attemt to write beyond RM boundary");
					return;
				}
				_sub_rm.poke(dst_offset, src, len);
			}
	};


	class Local_rm_service : public Service
	{
		private:

			Rpc_entrypoint     &_ep;
			Dataspace_registry &_ds_registry;

		public:

			Local_rm_service(Rpc_entrypoint &ep, Dataspace_registry &ds_registry)
			:
				Service(Rm_session::service_name()), _ep(ep),
				_ds_registry(ds_registry)
			{ }

			Genode::Session_capability session(const char *args)
			{
				addr_t start = Arg_string::find_arg(args, "start").ulong_value(~0UL);
				size_t size  = Arg_string::find_arg(args, "size").ulong_value(0);

				Rm_session_component *rm =
					new Rm_session_component(_ds_registry, start, size);

				Genode::Session_capability cap = _ep.manage(rm);

				_ds_registry.insert(new Rm_dataspace_info(*rm));

				return cap;
			}

			void upgrade(Genode::Session_capability, const char *args) { }

			void close(Genode::Session_capability)
			{
				PWRN("Local_rm_service::close not implemented, leaking memory");
			}
	};
}

#endif /* _NOUX__LOCAL_RM_SERVICE_H_ */
