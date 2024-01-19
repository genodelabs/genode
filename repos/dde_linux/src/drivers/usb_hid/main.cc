/*
 * \brief  C++ initialization, session, and client handling
 * \author Sebastian Sumpf
 * \date   2023-06-29
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
#include <base/registry.h>

#include <lx_emul/init.h>
#include <lx_emul/task.h>
#include <lx_emul/input_leds.h>
#include <lx_kit/env.h>

#include <genode_c_api/event.h>

/* C-interface */
#include <usb_hid.h>

#include <led_state.h>

using namespace Genode;

struct Task_handler
{
	task_struct                 *task;
	Signal_handler<Task_handler> handler;
	bool                         handling_signal { false };
	bool                         running         { true  };

	/*
	 * If the task is currently executing and the signal handler
	 * is called again via 'block_and_schedule()', we need to
	 * keep this information, so the task does not block at the
	 * end when a new signal already occurred.
	 *
	 * Initialized as true for the initial run of the task.
	 */
	bool _signal_pending { true };

	void handle_signal()
	{
		_signal_pending = true;
		lx_emul_task_unblock(task);
		handling_signal = true;
		Lx_kit::env().scheduler.execute();
		handling_signal = false;
	}

	bool signal_pending()
	{
		bool ret = _signal_pending;
		_signal_pending = false;
		return ret;
	}

	void block_and_schedule()
	{
		lx_emul_task_schedule(true);
	}

	void destroy_task()
	{
		running = false;
		lx_emul_task_unblock(task);
		/* will be unblocked by lx_user_destroy_usb_task */
		lx_emul_task_schedule(true);
	}

	Task_handler(Entrypoint & ep, task_struct *task)
	: task(task), handler(ep, *this, &Task_handler::handle_signal) { }

	/* non-copyable */
	Task_handler(const Task_handler&) = delete;
	Task_handler & operator=(const Task_handler&) = delete;
};


struct Leds
{
	Env &env;

	Attached_rom_dataspace &config_rom;

	task_struct *led_task { lx_user_new_usb_task(led_task_entry, this) };

	Task_handler led_task_handler { env.ep(), led_task };

	Usb::Led_state capslock { env, "capslock" },
	               numlock  { env, "numlock"  },
	               scrlock  { env, "scrlock"  };

	Leds(Env &env, Attached_rom_dataspace &config_rom)
	: env(env), config_rom(config_rom) { };

	/* non-copyable */
	Leds(const Leds&) = delete;
	Leds & operator=(const Leds&) = delete;

	void handle_config()
	{
		config_rom.update();
		Genode::Xml_node config =config_rom.xml();

		capslock.update(config, led_task_handler.handler);
		numlock .update(config, led_task_handler.handler);
		scrlock .update(config, led_task_handler.handler);
	}


	/**********
	 ** Task **
	 **********/

	static int led_task_entry(void *arg)
	{
		Leds &led = *reinterpret_cast<Leds *>(arg);

		while (true) {
			led.handle_config();

			lx_emul_input_leds_update(led.capslock.enabled(),
			                          led.numlock.enabled(),
			                          led.scrlock.enabled());

			led.led_task_handler.block_and_schedule();
		}
	}
};


struct Device : Registry<Device>::Element
{
	using Label = String<64>;

	Env   &env;
	Label  label;

	/*
	 * Dedicated allocator per device to notice dangling
	 * allocations on device destruction.
	 */
	Allocator_avl alloc { &Lx_kit::env().heap };

	task_struct *state_task { lx_user_new_usb_task(state_task_entry, this) };
	task_struct *urb_task   { lx_user_new_usb_task(urb_task_entry, this)   };

	Task_handler state_task_handler { env.ep(), state_task };
	Task_handler urb_task_handler   { env.ep(), urb_task   };

	genode_usb_client_handle_t usb_handle {
		genode_usb_client_create(genode_env_ptr(env),
		                         genode_allocator_ptr(Lx_kit::env().heap),
		                         genode_range_allocator_ptr(alloc),
		                         label.string(),
		                         genode_signal_handler_ptr(state_task_handler.handler)) };

	bool updated    { true };
	bool registered { false };

	void *lx_device_handle { nullptr };

	Device(Env &env, Registry<Device> &registry, Label label)
	:
		Registry<Device>::Element(registry, *this),
		env(env), label(label)
	{
		genode_usb_client_sigh_ack_avail(usb_handle,
		                                 genode_signal_handler_ptr(urb_task_handler.handler));
	}

	~Device()
	{
		genode_usb_client_destroy(usb_handle,
		                          genode_allocator_ptr(Lx_kit::env().heap));

		state_task_handler.destroy_task();
		urb_task_handler.destroy_task();
	}

	/* non-copyable */
	Device(const Device&) = delete;
	Device & operator=(const Device&) = delete;

	void register_device()
	{
		registered = true;
		lx_device_handle = lx_emul_usb_client_register_device(usb_handle, label.string());
		if (!lx_device_handle) registered = false;
	}

	void unregister_device()
	{
		lx_emul_usb_client_unregister_device(usb_handle, lx_device_handle);
		registered = false;
	}

	bool deinit() { return !registered &&
	                       !state_task_handler.handling_signal &&
	                       !urb_task_handler.handling_signal; }

	/**********
	 ** Task **
	 **********/

	static int state_task_entry(void *arg)
	{
		Device &device = *reinterpret_cast<Device *>(arg);

		while (device.state_task_handler.running) {
			while (device.state_task_handler.signal_pending()) {
				if (genode_usb_client_plugged(device.usb_handle) && !device.registered)
					device.register_device();

				if (!genode_usb_client_plugged(device.usb_handle) && device.registered)
					device.unregister_device();
			}
			device.state_task_handler.block_and_schedule();
		}
		lx_user_destroy_usb_task(device.state_task_handler.task);
		return 0;
	}

	static int urb_task_entry(void *arg)
	{
		Device &device = *reinterpret_cast<Device *>(arg);

		while (device.urb_task_handler.running) {
			if (device.registered)
				genode_usb_client_execute_completions(device.usb_handle);

			device.urb_task_handler.block_and_schedule();
		}
		lx_user_destroy_usb_task(device.urb_task_handler.task);
		return 0;
	}
};


struct Driver
{
	Env &env;

	Task_handler task_handler;

	Heap &heap { Lx_kit::env().heap };

	bool          use_report  { false };

	Constructible<Attached_rom_dataspace> report_rom { };

	Attached_rom_dataspace config_rom { env, "config" };

	Leds leds { env, config_rom };

	Registry<Device> devices { };

	Driver(Env &env, task_struct *task)
	: env(env), task_handler(env.ep(), task)
	{
		try {
			Xml_node config = config_rom.xml();
			use_report      = config.attribute_value("use_report", false);
		} catch(...) { }

		if (use_report)
			warning("use compatibility mode: ",
			        "will claim all HID devices from USB report");
	}

	void scan_report()
	{
		if (!report_rom.constructed()) {
			report_rom.construct(env, "report");
			report_rom->sigh(task_handler.handler);
		}

		report_rom->update();

		devices.for_each([&] (Device & d) { d.updated = false; });

		try {
			Xml_node report_node = report_rom->xml();
			report_node.for_each_sub_node([&] (Xml_node & dev_node)
			{
				unsigned long c = 0;
				dev_node.attribute("class").value(c);
				if (c != 0x3 /* USB_CLASS_HID */) return;

				Device::Label label;
				dev_node.attribute("label").value(label);

				bool found = false;

				devices.for_each([&] (Device & d) {
					if (d.label == label) d.updated = found = true; });

				if (!found) new (heap) Device(env, devices, label);
			});
		} catch(...) {
			error("Error parsing USB devices report");
			throw;
		};

		devices.for_each([&] (Device & d) {
			if (!d.updated && d.deinit()) {
				destroy(heap, &d);
			}
		});
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

	genode_event_init(genode_env_ptr(env),
	                  genode_allocator_ptr(Lx_kit::env().heap));

	lx_emul_start_kernel(nullptr);
}


/**********
 ** Task **
 **********/

int lx_user_main_task(void *data)
{
	task_struct *task = *static_cast<task_struct **>(data);

	static Driver driver { Lx_kit::env().env, task };

	for (;;) {
		while (driver.task_handler.signal_pending()) {
			if (!driver.use_report)
				static Device dev(driver.env, driver.devices, Device::Label(""));
			else
				driver.scan_report();
		}
		driver.task_handler.block_and_schedule();
	}
	return 0;
}
