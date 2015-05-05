/*
 * \brief  CAP root interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_ROOT_H_
#define _CORE__INCLUDE__CAP_ROOT_H_

/* Genode includes */
#include <root/component.h>

/* core includes */
#include <cap_session_component.h>

namespace Genode {

	class Cap_root : public Root_component<Cap_session_component>
	{
		private:

			Allocator *_md_alloc;

		protected:

			Cap_session_component *_create_session(const char *args) {
				return new (md_alloc()) Cap_session_component(_md_alloc, args); }

			void _upgrade_session(Cap_session_component *c, const char *args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				c->upgrade_ram_quota(ram_quota);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Cap_root(Rpc_entrypoint *session_ep,
			         Allocator      *md_alloc)
			:
				Root_component<Cap_session_component>(session_ep, md_alloc),
				_md_alloc(md_alloc)
			{ }
	};
}

#endif /* _CORE__INCLUDE__CAP_ROOT_H_ */
