/**
 * \brief  RM root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RM_ROOT_H_
#define _CORE__INCLUDE__RM_ROOT_H_

/* Genode */
#include <root/component.h>

/* Core */
#include <rm_session_component.h>

namespace Genode {

	class Rm_root : public Root_component<Rm_session_component>
	{
		private:

			Rpc_entrypoint         *_ds_ep;
			Rpc_entrypoint         *_thread_ep;
			Allocator              *_md_alloc;

			enum { PAGER_STACK_SIZE = 2*4096 };
			Pager_activation<PAGER_STACK_SIZE> _pager_thread;

			Pager_entrypoint        _pager_ep;

			addr_t                  _vm_start;
			size_t                  _vm_size;

		protected:

			Rm_session_component *_create_session(const char *args)
			{
				addr_t start     = Arg_string::find_arg(args, "start").ulong_value(~0UL);
				size_t size      = Arg_string::find_arg(args, "size").ulong_value(0);
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

				return new (md_alloc())
				       Rm_session_component(_ds_ep,
				                            _thread_ep,
				                            this->ep(),
				                            _md_alloc, ram_quota,
				                            &_pager_ep,
				                            start == ~0UL ? _vm_start : start,
				                            size  ==  0   ? _vm_size  : size);
			}

			Session_capability session(Root::Session_args const &args, Affinity const &affinity)
			{
				Session_capability cap = Root_component<Rm_session_component>::session(args, affinity);

				/* lookup rm_session_component object */
				Object_pool<Rm_session_component>::Guard rm_session(ep()->lookup_and_lock(cap));
				if (!rm_session)
					/* should never happen */
					return cap;

				/**
				 * Assign rm_session capability to dataspace component. It can
				 * not be done beforehand because the dataspace_component is
				 * constructed before the rm_session
				 */
				if (rm_session->dataspace_component())
					rm_session->dataspace_component()->sub_rm_session(rm_session->cap());
				return cap;
			}

			void _upgrade_session(Rm_session_component *rm, const char *args)
			{
				size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				rm->upgrade_ram_quota(ram_quota);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep   entry point for managing RM session objects
			 * \param ds_ep        entry point for managing dataspaces
			 * \param thread_ep    entry point for managing threads
			 * \param md_alloc     meta data allocator to be used by root component
			 * \param cap_session  allocator for pager-object capabilities
			 * \param vm_start     begin of virtual memory (default value)
			 * \param vm_size      size of virtual memory (default value)
			 */
			Rm_root(Rpc_entrypoint *session_ep,
			        Rpc_entrypoint *ds_ep,
			        Rpc_entrypoint *thread_ep,
			        Allocator      *md_alloc,
			        Cap_session    *cap_session,
			        addr_t          vm_start,
			        size_t          vm_size)
			:
				Root_component<Rm_session_component>(session_ep, md_alloc),
				_ds_ep(ds_ep), _thread_ep(thread_ep), _md_alloc(md_alloc),
				_pager_thread(), _pager_ep(cap_session, &_pager_thread),
				_vm_start(vm_start), _vm_size(vm_size) { }

			/**
			 * Return pager entrypoint
			 */
			Pager_entrypoint *pager_ep() { return &_pager_ep; }
	};
}

#endif /* _CORE__INCLUDE__RM_ROOT_H_ */
