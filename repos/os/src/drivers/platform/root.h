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

#ifndef _SRC__DRIVERS__PLATFORM__ROOT_H_
#define _SRC__DRIVERS__PLATFORM__ROOT_H_

#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <root/component.h>
#include <session_component.h>

#include <device.h>

namespace Driver { class Root; }

class Driver::Root : public Root_component<Session_component>
{
	public:

		Root(Env                          & env,
		     Sliced_heap                  & sliced_heap,
		     Attached_rom_dataspace const & config,
		     Device_model                 & devices,
		     bool const                     iommu);

		void update_policy();

	private:

		Session_component * _create_session(const char * args) override;

		void _upgrade_session(Session_component *, const char *) override;

		Env                          & _env;
		Attached_rom_dataspace const & _config;
		Device_model                 & _devices;
		bool const                     _iommu;
		Registry<Session_component>    _sessions {};
};

#endif /* _SRC__DRIVERS__PLATFORM__ROOT_H_ */
