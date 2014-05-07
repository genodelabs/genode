/*
 * \brief  RAM root interface
 * \author Norman Feske
 * \date   2006-05-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RAM_ROOT_H_
#define _RAM_ROOT_H_

#include <root/component.h>

#include "ram_session_component.h"

namespace Genode {

	class Ram_root : public Root_component<Ram_session_component>
	{
		protected:

			Ram_session_component *_create_session(const char *args)
			{
				return new (md_alloc())
					Ram_session_component(args);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing ram session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Ram_root(Rpc_entrypoint *session_ep,
			         Allocator      *md_alloc)
			: Root_component<Ram_session_component>(session_ep, md_alloc)
			{ }
	};
}

#endif /* _RAM_ROOT_H_ */
