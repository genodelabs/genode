/*
 * \brief  Loader root interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOADER_ROOT_H_
#define _LOADER_ROOT_H_

/* Genode includes */
#include <root/component.h>
#include <util/arg_string.h>

/* local includes */
#include <loader_session_component.h>

namespace Loader {

	class Root : public Genode::Root_component<Loader::Session_component>
	{
		protected:

			Loader::Session_component *_create_session(const char *args)
			{
				using namespace Genode;
				size_t ds_size = Arg_string::find_arg(args, "ds_size").long_value(0);
				return new (md_alloc()) Loader::Session_component(ds_size);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  entry point for managing ram session objects
			 * \param md_alloc    meta-data allocator to be used by root component
			 */
			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			: Genode::Root_component<Loader::Session_component>(session_ep, md_alloc) { }
	};
}

#endif /* _LOADER_ROOT_H_ */
