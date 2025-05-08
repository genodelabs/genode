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

/* core includes */
#include <rm_session_component.h>
#include <rpc_cap_factory.h>

namespace Core { class Rm_root; }


class Core::Rm_root : public Root_component<Rm_session_component>
{
	private:

		Ram_allocator    &_ram_alloc;
		Local_rm         &_local_rm;
		Pager_entrypoint &_pager_ep;

	protected:

		Create_result _create_session(const char *args) override
		{
			return *new (md_alloc())
			       Rm_session_component(*this->ep(),
			                            session_resources_from_args(args),
			                            session_label_from_args(args),
			                            session_diag_from_args(args),
			                            _ram_alloc, _local_rm, _pager_ep);
		}

		void _upgrade_session(Rm_session_component &rm, const char *args) override
		{
			rm.upgrade(ram_quota_from_args(args));
			rm.upgrade(cap_quota_from_args(args));
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep   entry point for managing RM session objects
		 * \param md_alloc     meta data allocator to for session objects
		 * \param ram_alloc    RAM allocator used for session-internal
		 *                     allocations
		 * \param pager_ep     pager entrypoint
		 */
		Rm_root(Rpc_entrypoint   &session_ep,
		        Allocator        &md_alloc,
		        Ram_allocator    &ram_alloc,
		        Local_rm         &local_rm,
		        Pager_entrypoint &pager_ep)
		:
			Root_component<Rm_session_component>(&session_ep, &md_alloc),
			_ram_alloc(ram_alloc), _local_rm(local_rm), _pager_ep(pager_ep)
		{ }
};

#endif /* _CORE__INCLUDE__RM_ROOT_H_ */
