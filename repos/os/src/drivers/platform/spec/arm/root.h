/*
 * \brief  Platform driver for ARM root component
 * \author Stefan Kalkowski
 * \date   2020-04-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__ROOT_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__ROOT_H_

#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <root/component.h>
#include <session_component.h>

#include <device.h>

namespace Driver { class Root; }

class Driver::Root : public Genode::Root_component<Driver::Session_component>
{
	public:

		Root(Genode::Env                    & env,
		     Genode::Allocator              & alloc,
		     Genode::Attached_rom_dataspace & config,
		     Device_model                   & devices);

		void update_policy();

	private:

		Session_component * _create_session(const char * args) override;

		void _upgrade_session(Session_component *, const char *) override;

		Genode::Env                       & _env;
		Genode::Attached_rom_dataspace    & _config;
		Driver::Device_model              & _devices;
		Genode::Registry<Session_component> _sessions {};
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__ROOT_H_ */
