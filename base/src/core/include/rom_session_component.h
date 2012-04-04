/*
 * \brief  Core-specific instance of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
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

namespace Genode {

	class Rom_session_component : public Rpc_object<Rom_session>
	{
		private:

			Rom_module              *_rom_module;
			char                     _fname[32];
			Dataspace_component      _ds;
			Rpc_entrypoint          *_ds_ep;
			Rom_dataspace_capability _ds_cap;

			Rom_module * _find_rom(Rom_fs *rom_fs, const char *args)
			{
				/* extract filename from session arguments */
				Arg_string::find_arg(args, "filename").string(_fname, sizeof(_fname), "");

				/* find ROM module for file name */
				return rom_fs->find(_fname);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param rom_fs  ROM filesystem
			 * \param ds_ep   entry point to manage the dataspace
			 *                corresponding the rom session
			 * \param args    session-construction arguments, in
			 *                particular the filename
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
