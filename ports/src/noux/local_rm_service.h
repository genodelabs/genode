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

	class Rm_dataspace_info : public Dataspace_info
	{
		private:

			Rm_session_component * const _sub_rm;
			Rpc_entrypoint              &_ep;
			Rm_session_capability        _rm_cap;

		public:

			/**
			 * Constructor
			 *
			 * \param sub_rm  pointer to 'Rm_session_component' that belongs
			 *                to the 'Rm_dataspace_info' object
			 * \param ep      entrypoint to manage the 'Rm_session_component'
			 *
			 * The ownership of the pointed-to object gets transferred to the
			 * 'Rm_dataspace_info' object. I.e., 'sub_rm' will get destructed
			 * on the destruction of its corresponding 'Rm_dataspace_info'
			 * object. Also, 'Rm_dataspace_info' takes care of associating
			 * 'sub_rm' with 'ep'.
			 */
			Rm_dataspace_info(Rm_session_component *sub_rm,
			                  Rpc_entrypoint &ep)
			:
				Dataspace_info(sub_rm->dataspace()),
				_sub_rm(sub_rm), _ep(ep), _rm_cap(_ep.manage(_sub_rm))
			{ }

			~Rm_dataspace_info()
			{
				_ep.dissolve(_sub_rm);
				destroy(env()->heap(), _sub_rm);
			}

			Rm_session_capability rm_cap() { return _rm_cap; }

			Dataspace_capability fork(Ram_session_capability ram,
			                          Dataspace_registry  &ds_registry,
			                          Rpc_entrypoint      &ep)
			{
				Rm_session_component *new_sub_rm =
					new Rm_session_component(ds_registry, 0, size());

				Rm_dataspace_info *rm_info = new Rm_dataspace_info(new_sub_rm, ep);

				_sub_rm->replay(ram, rm_info->rm_cap(), ds_registry, ep);

				ds_registry.insert(rm_info);

				return new_sub_rm->dataspace();
			}

			void poke(addr_t dst_offset, void const *src, size_t len)
			{
				if ((dst_offset >= size()) || (dst_offset + len > size())) {
					PERR("illegal attemt to write beyond RM boundary");
					return;
				}
				_sub_rm->poke(dst_offset, src, len);
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

				Rm_dataspace_info *info = new Rm_dataspace_info(rm, _ep);
				_ds_registry.insert(info);
				return info->rm_cap();
			}

			void upgrade(Genode::Session_capability, const char *args) { }

			void close(Genode::Session_capability session)
			{
				Rm_session_component *rm_session =
					dynamic_cast<Rm_session_component *>(_ep.obj_by_cap(session));

				if (!rm_session) {
					PWRN("Unexpected call of close with non-RM-session argument");
					return;
				}

				/* use RM dataspace as key to obtain the dataspace info object */
				Dataspace_capability ds_cap = rm_session->dataspace();

				/* release dataspace info */
				Dataspace_info *info = _ds_registry.lookup_info(ds_cap);
				if (info) {
					_ds_registry.remove(info);
					destroy(env()->heap(), info);
				} else {
					PWRN("Could not lookup dataspace info for local RM session");
				}
			}
	};
}

#endif /* _NOUX__LOCAL_RM_SERVICE_H_ */
