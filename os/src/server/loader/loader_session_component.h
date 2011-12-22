/*
 * \brief  Instance of the Loader session interface
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOADER_SESSION_COMPONENT_H_
#define _LOADER_SESSION_COMPONENT_H_

/* Genode includes */
#include <loader_session/loader_session.h>
#include <rom_session/connection.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <nitpicker_view/capability.h>
#include <base/rpc_server.h>

/* local includes */
#include <tar_server_child.h>
#include <loader_child.h>

namespace Loader {

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Dataspace_capability _input_ds;

			Genode::Dataspace_capability _tar_ds;
			Genode::Rom_connection      *_tar_rom;

			Genode::Rom_connection      *_tar_server_child_elf;
			Genode::Dataspace_capability _tar_server_child_elf_ds;
			Genode::Service_registry     _tar_server_child_parent_services;
			Tar_server_child            *_tar_server_child;

			Genode::Rom_connection      *_loader_child_elf;
			Genode::Dataspace_capability _loader_child_elf_ds;
			Genode::Service_registry     _loader_child_parent_services;
			Loader::Child               *_loader_child;

			/* cap session for allocating capabilities for parent interfaces */
			Genode::Cap_connection _cap_session;

			Genode::size_t _ram_quota;

			Genode::Timed_semaphore _ready_sem;

		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::size_t input_ds_size);

			/**
			 * Destructor
			 */
			~Session_component();


			/******************************
			 ** Loader session interface **
			 ******************************/

			Genode::Dataspace_capability dataspace() { return _input_ds; }

			void start(Start_args const &args,
			           int max_width, int max_height,
			           Genode::Alarm::Time timeout,
			           Name const &name);

			Nitpicker::View_capability view(int *w, int *h, int *buf_x, int *buf_y);
	};
}

#endif /* _LOADER_SESSION_COMPONENT_H_ */
