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

		Usb_session(Ram_dataspace_capability  tx_ds,
		            Entrypoint               &ep,
		            Region_map               &rm)
		:
			Session_rpc_object { tx_ds, ep.rpc_ep(), rm }
		{ }

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

		Usb_session *_create_session(char const *args) override
		{
			size_t const ram_quota {
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0) };

			size_t const tx_buf_size {
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0) };

			size_t const session_size {
				max<size_t>(4096, sizeof(Usb_session)) };

			if (ram_quota < session_size + tx_buf_size) {
				throw Insufficient_ram_quota { };
			}
			Ram_dataspace_capability tx_ds { _env.ram().alloc(tx_buf_size) };
			return new (md_alloc())
				Usb_session { tx_ds, _env.ep(), _env.rm() };
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
