/*
 * \brief  C++ initialization, session, and client handling
 * \author Sebastian Sumpf
 * \date   2023-07-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>

#include <genode_c_api/uplink.h>
#include <genode_c_api/mac_address_reporter.h>

#include <nic_session/nic_session.h>

#include <lx_kit/env.h>
#include <lx_emul/task.h>
#include <lx_emul/init.h>

/* C-interface */
#include <usb_net.h>

using namespace Genode;

extern task_struct *user_task_struct_ptr;
extern bool force_uplink_destroy;
extern bool use_mac_address;
extern unsigned char mac_address[6];

struct Device
{
	Env   &env;

	Attached_rom_dataspace config_rom { env, "config" };
	unsigned long usb_config { 0 };

	/*
	 * Dedicated allocator per device to notice dangling
	 * allocations on device destruction.
	 */
	Allocator_avl alloc { &Lx_kit::env().heap };

	task_struct *state_task { lx_user_new_usb_task(state_task_entry, this) };
	task_struct *urb_task   { lx_user_new_usb_task(urb_task_entry, this)   };

	Signal_handler<Device> task_state_handler { env.ep(), *this, &Device::handle_task_state };
	Signal_handler<Device> urb_handler        { env.ep(), *this, &Device::handle_urb        };

	genode_usb_client_handle_t usb_handle {
		genode_usb_client_create(genode_env_ptr(env),
		                         genode_allocator_ptr(Lx_kit::env().heap),
		                         genode_range_allocator_ptr(alloc),
		                         "",
		                         genode_signal_handler_ptr(task_state_handler)) };

	Signal_handler<Device> nic_handler    { env.ep(), *this, &Device::handle_nic    };
	Signal_handler<Device> config_handler { env.ep(), *this, &Device::handle_config };

	bool registered { false };

	void *lx_device_handle { nullptr };

	Device(Env &env)
	:
		env(env)
	{
		genode_usb_client_sigh_ack_avail(usb_handle,
		                                 genode_signal_handler_ptr(urb_handler));

		genode_mac_address_reporter_init(env, Lx_kit::env().heap);

		genode_uplink_init(genode_env_ptr(env),
		                   genode_allocator_ptr(Lx_kit::env().heap),
		                   genode_signal_handler_ptr(nic_handler));

		config_rom.sigh(config_handler);
		handle_config();
	}

	/* non-copyable */
	Device(const Device&) = delete;
	Device & operator=(const Device&) = delete;

	void register_device()
	{
		registered = true;
		lx_device_handle = lx_emul_usb_client_register_device(usb_handle, "usb_nic");
		if (!lx_device_handle) {
			registered = false;
			return;
		}

		if (usb_config != 0)
			lx_emul_usb_client_set_configuration(usb_handle, lx_device_handle, usb_config);

	}

	void unregister_device()
	{
		force_uplink_destroy = true;
		lx_emul_usb_client_unregister_device(usb_handle, lx_device_handle);
		registered = false;
		force_uplink_destroy = false;
	}

	void handle_task_state()
	{
		lx_emul_task_unblock(state_task);
		Lx_kit::env().scheduler.execute();
	}

	void handle_urb()
	{
		lx_emul_task_unblock(urb_task);
		Lx_kit::env().scheduler.execute();
		genode_uplink_notify_peers();
	}

	void handle_nic()
	{
		if (!user_task_struct_ptr)
			return;

		lx_emul_task_unblock(user_task_struct_ptr);
		Lx_kit::env().scheduler.execute();
	}

	void handle_config()
	{
		config_rom.update();
		genode_mac_address_reporter_config(config_rom.xml());

		/* read USB configuration setting */
		unsigned long config = config_rom.xml().attribute_value("configuration", 0ul);
		if (registered && config != 0 && config != usb_config)
			lx_emul_usb_client_set_configuration(usb_handle, lx_device_handle, config);

		usb_config = config;

		/* retrieve possible MAC */
		Nic::Mac_address mac;
		try {
			Xml_node::Attribute mac_node = config_rom.xml().attribute("mac");
			mac_node.value(mac);
			mac.copy(mac_address);
			use_mac_address = true;
			log("Trying to use configured mac: ", mac);
		} catch (...) {
			use_mac_address = false;
		}
	}


	/**********
	 ** Task **
	 **********/

	static int state_task_entry(void *arg)
	{
		Device &device = *reinterpret_cast<Device *>(arg);

		while (true) {
			if (genode_usb_client_plugged(device.usb_handle) && !device.registered)
				device.register_device();

			if (!genode_usb_client_plugged(device.usb_handle) && device.registered)
				device.unregister_device();
			lx_emul_task_schedule(true);
		}

		return 0;
	}

	static int urb_task_entry(void *arg)
	{
		Device &device = *reinterpret_cast<Device *>(arg);

		while (true) {
			if (device.registered) {
				genode_usb_client_execute_completions(device.usb_handle);
			}

			lx_emul_task_schedule(true);
		}

		return 0;
	}
};


struct Main
{
	Env &env;

	Signal_handler<Main> signal_handler { env.ep(), *this, &Main::handle_signal };

	Main(Env &env) : env(env) { }

	void handle_signal()
	{
		Lx_kit::env().scheduler.execute();
	}
};


void Component::construct(Env & env)
{
	static Main main { env };
	Lx_kit::initialize(env, main.signal_handler);

	env.exec_static_constructors();

	lx_emul_start_kernel(nullptr);
}


/**********
 ** Task **
 **********/

int lx_user_main_task(void *)
{
	/* one device only */
	static Device dev(Lx_kit::env().env);

	return 0;
}
