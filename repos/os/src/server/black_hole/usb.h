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
#include <usb_session/rpc_object.h>

namespace Black_hole {

	using namespace Genode;
	using namespace Usb;

	class Usb_session;
	class Usb_root;
}


class Black_hole::Usb_session : public Usb::Session_rpc_object
{
	public:

		void sigh_state_change(Signal_context_capability /* sigh */) override { }

		bool plugged() override { return false; }

		void config_descriptor(Device_descriptor * /* device_descr */,
		                       Config_descriptor * /* config_descr */) override { }

		unsigned alt_settings(unsigned /* index */) override { return 0; }

		void interface_descriptor(unsigned               /* index */,
		                          unsigned               /* alt_setting */,
		                          Interface_descriptor * /* interface_descr */) override { }

		bool interface_extra(unsigned          /* index */,
		                     unsigned          /* alt_setting */,
		                     Interface_extra * /* interface_data */) override { return false; }

		void endpoint_descriptor(unsigned               /* interface_num */,
		                         unsigned               /* alt_setting */,
		                         unsigned               /* endpoint_num */,
		                         Endpoint_descriptor  * /* endpoint_descr */) override { }

		void claim_interface(unsigned /* interface_num */) override { }

		void release_interface(unsigned /* interface_num */) override { }
};


class Black_hole::Usb_root : public Root_component<Usb_session>
{
	private:

		Env &_env;

	protected:

		Usb_session *_create_session(char const * /* args */) override
		{
			/*
			 * FIXME
			 *
			 * Currently, we're fine with a service that is routable but
			 * not usable. In the long term, this exception should be removed
			 * and a session object should be returned that can be used as if
			 * it was a real USB session.
			 */
			throw Service_denied { };
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
