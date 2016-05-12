/*
 * \brief  Core-specific instance of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_

#include <rom_fs.h>
#include <base/rpc_server.h>
#include <dataspace_component.h>
#include <rom_session/rom_session.h>
#include <base/session_label.h>

namespace Genode {

	class Rom_session_component : public Rpc_object<Rom_session>
	{
		private:

			Rom_module              *_rom_module;
			Dataspace_component      _ds;
			Rpc_entrypoint          *_ds_ep;
			Rom_dataspace_capability _ds_cap;

			Rom_module * _find_rom(Rom_fs *rom_fs, const char *args)
			{
				/* extract label */
				Session_label const label = label_from_args(args);

				/* find ROM module for trailing label element */
				return rom_fs->find(label.last_element().string());
			}

		public:

			/**
			 * Constructor
			 *
			 * \param rom_fs  ROM filesystem
			 * \param ds_ep   entry point to manage the dataspace
			 *                corresponding the rom session
			 * \param args    session-construction arguments
			 */
			Rom_session_component(Rom_fs            *rom_fs,
			                      Rpc_entrypoint    *ds_ep,
			                      const char        *args);

			/**
			 * Destructor
			 */
			~Rom_session_component();


			/***************************
			 ** Rom session interface **
			 ***************************/

			Rom_dataspace_capability dataspace() { return _ds_cap; }
			void sigh(Signal_context_capability) { }
	};
}

#endif /* _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_ */
