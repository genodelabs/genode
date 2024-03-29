/*
 * \brief  Core-specific instance of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <dataspace_component.h>
#include <rom_session/rom_session.h>
#include <base/session_label.h>

/* core includes */
#include <rom_fs.h>

namespace Core { class Rom_session_component; }


class Core::Rom_session_component : public Rpc_object<Rom_session>
{
	private:

		Rom_module const * const _rom_module = nullptr;
		Dataspace_component      _ds;
		Rpc_entrypoint          &_ds_ep;
		Rom_dataspace_capability _ds_cap;

		Rom_module const &_find_rom(Rom_fs &rom_fs, const char *args)
		{
			return rom_fs.with_element(label_from_args(args).last_element(),

				[&] (Rom_module const &rom) -> Rom_module const & {
					return rom; },

				[&] () -> Rom_module const & {
					throw Service_denied(); });
		}

		/*
		 * Noncopyable
		 */
		Rom_session_component(Rom_session_component const &);
		Rom_session_component &operator = (Rom_session_component const &);

	public:

		/**
		 * Constructor
		 *
		 * \param rom_fs  ROM filesystem
		 * \param ds_ep   entry point to manage the dataspace
		 *                corresponding the rom session
		 * \param args    session-construction arguments
		 */
		Rom_session_component(Rom_fs         &rom_fs,
		                      Rpc_entrypoint &ds_ep,
		                      const char     *args);

		/**
		 * Destructor
		 */
		~Rom_session_component();


		/***************************
		 ** Rom session interface **
		 ***************************/

		Rom_dataspace_capability dataspace() override { return _ds_cap; }
		void sigh(Signal_context_capability) override { }
};

#endif /* _CORE__INCLUDE__ROM_SESSION_COMPONENT_H_ */
