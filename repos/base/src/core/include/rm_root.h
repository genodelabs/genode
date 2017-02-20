/*
 * \brief  RM root interface
 * \author Norman Feske
 * \date   2016-04-17
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RM_ROOT_H_
#define _CORE__INCLUDE__RM_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* core-local includes */
#include <rm_session_component.h>
#include <rpc_cap_factory.h>

namespace Genode { class Rm_root; }


class Genode::Rm_root : public Root_component<Rm_session_component>
{
	private:

		Pager_entrypoint &_pager_ep;

	protected:

		Rm_session_component *_create_session(const char *args)
		{
			size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			return new (md_alloc())
			       Rm_session_component(*this->ep(), *md_alloc(), _pager_ep, ram_quota);
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
		 * \param md_alloc     meta data allocator to be used by root component
		 * \param pager_ep     pager entrypoint
		 */
		Rm_root(Rpc_entrypoint   *session_ep,
		        Allocator        *md_alloc,
		        Pager_entrypoint &pager_ep)
		:
			Root_component<Rm_session_component>(session_ep, md_alloc),
			_pager_ep(pager_ep)
		{ }
};

#endif /* _CORE__INCLUDE__RM_ROOT_H_ */
