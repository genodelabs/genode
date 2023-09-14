/*
 * \brief  Usb session component and root
 * \author Martin Stein
 * \date   2022-02-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _USB_H_
#define _USB_H_

/* base includes */
#include <root/component.h>

/* os includes */
#include <usb_session/usb_session.h>
#include <os/dynamic_rom_session.h>

namespace Black_hole {

	using namespace Genode;
	using namespace Usb;

	class Usb_session;
	class Usb_root;
}


class Black_hole::Usb_session : public Session_object<Usb::Session>,
                                private Dynamic_rom_session::Xml_producer
{
	private:

		Env                &_env;
		Dynamic_rom_session _rom_session { _env.ep(), _env.ram(),
		                                   _env.rm(), *this };

	public:

		Usb_session(Env             &env,
		            Label     const &label,
		            Resources const &resources,
		            Diag      const &diag)

		:
			Session_object<Usb::Session>(env.ep(), resources, label, diag),
			Dynamic_rom_session::Xml_producer("devices"),
			_env(env) { }

		Rom_session_capability devices_rom() override {
			return _rom_session.cap(); }

		Device_capability acquire_device(Device_name const &) override {
			return Device_capability(); }

		Device_capability acquire_single_device() override {
			return Device_capability(); }

		void release_device(Device_capability) override {}


		/*******************************************
		 ** Dynamic_rom_session::Xml_producer API **
		 *******************************************/

		void produce_xml(Xml_generator &) override {}
};


class Black_hole::Usb_root : public Root_component<Usb_session>
{
	private:

		Env &_env;

	protected:

		Usb_session *_create_session(char const *args) override
		{
			return new (md_alloc())
				Usb_session { _env,
				              session_label_from_args(args),
				              session_resources_from_args(args),
				              session_diag_from_args(args) };
		}

	public:

		Usb_root(Env       &env,
		         Allocator &alloc)
		:
			Root_component { env.ep(), alloc },
			_env           { env }
		{ }
};

#endif /* _USB_H_ */
