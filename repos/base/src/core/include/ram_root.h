/*
 * \brief  RAM root interface
 * \author Norman Feske
 * \date   2006-05-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RAM_ROOT_H_
#define _CORE__INCLUDE__RAM_ROOT_H_

#include <root/component.h>

#include "ram_session_component.h"

namespace Genode {

	class Ram_root : public Root_component<Ram_session_component>
	{
		private:

			Rpc_entrypoint  &_ep;
			Range_allocator &_phys_alloc;
			Region_map      &_local_rm;

			static Ram_session_component::Phys_range phys_range_from_args(char const *args)
			{
				addr_t const start = Arg_string::find_arg(args, "phys_start").ulong_value(0);
				addr_t const size  = Arg_string::find_arg(args, "phys_size").ulong_value(0);
				addr_t const end   = start + size - 1;

				return (start <= end) ? Ram_session_component::Phys_range { start, end }
				                      : Ram_session_component::any_phys_range();
			}

		protected:

			Ram_session_component *_create_session(const char *args)
			{
				return new (md_alloc())
					Ram_session_component(_ep,
					                      session_resources_from_args(args),
					                      session_label_from_args(args),
					                      session_diag_from_args(args),
					                      _phys_alloc, _local_rm,
					                      phys_range_from_args(args));
			}

			void _upgrade_session(Ram_session_component *ram, const char *args)
			{
				ram->Ram_quota_guard::upgrade(ram_quota_from_args(args));
				ram->Cap_quota_guard::upgrade(cap_quota_from_args(args));
				ram->session_quota_upgraded();
			}

		public:

			Ram_root(Rpc_entrypoint  &ep,
			         Range_allocator &phys_alloc,
			         Region_map      &local_rm,
			         Allocator       &md_alloc)
			:
				Root_component<Ram_session_component>(&ep, &md_alloc),
				_ep(ep), _phys_alloc(phys_alloc), _local_rm(local_rm)
			{ }
	};
}

#endif /* _CORE__INCLUDE__RAM_ROOT_H_ */
