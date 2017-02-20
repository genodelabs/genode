/*
 * \brief  PD root interface
 * \author Christian Helmuth
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PD_ROOT_H_
#define _CORE__INCLUDE__PD_ROOT_H_

/* Genode */
#include <root/component.h>

/* Core */
#include <pd_session_component.h>

namespace Genode {
	class Pd_root;
}


class Genode::Pd_root : public Genode::Root_component<Genode::Pd_session_component>
{
	private:

		Rpc_entrypoint   &_thread_ep;
		Pager_entrypoint &_pager_ep;
		Allocator        &_md_alloc;

	protected:

		Pd_session_component *_create_session(const char *args)
		{
			/* XXX use separate entrypoint for PD sessions */
			return new (md_alloc()) Pd_session_component(_thread_ep,
			                                             _thread_ep,
			                                             _thread_ep,
			                                             _md_alloc,
			                                             _pager_ep, args);
		}

		void _upgrade_session(Pd_session_component *p, const char *args)
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			p->upgrade_ram_quota(ram_quota);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entry point for managing pd session objects
		 * \param thread_ep   entry point for managing threads
		 * \param md_alloc    meta-data allocator to be used by root component
		 */
		Pd_root(Rpc_entrypoint   *session_ep,
		        Rpc_entrypoint   *thread_ep,
		        Pager_entrypoint &pager_ep,
		        Allocator        *md_alloc)
		: Root_component<Pd_session_component>(session_ep, md_alloc),
		  _thread_ep(*thread_ep), _pager_ep(pager_ep), _md_alloc(*md_alloc) { }
};

#endif /* _CORE__INCLUDE__PD_ROOT_H_ */
