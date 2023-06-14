/*
 * \brief  USB HID driver
 * \author Stefan Kalkowski
 * \date   2018-06-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__USB_HID__DRIVER_H_
#define _SRC__DRIVERS__USB_HID__DRIVER_H_

#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <usb_session/connection.h>
#include <event_session/connection.h>
#include <legacy/lx_kit/scheduler.h>

struct usb_device_id;
struct usb_interface;
struct usb_device;

struct Driver
{
	using Label = Genode::String<64>;

	struct Task
	{
		Lx::Task                      task;
		Genode::Signal_handler<Task>  handler;
		bool                          handling_signal { false };

		/*
		 * If the task is currently executing and the signal handler
		 * is called again via 'block_and_schedule()', we need to
		 * keep this information, so the task does not block at the
		 * end when a new signal already occurred.
		 *
		 * Initialized as true for the initial run of the task.
		 */
		bool                         _signal_pending { true };
	
		void handle_signal()
		{
			_signal_pending = true;
			task.unblock();
			handling_signal = true;
			Lx::scheduler().schedule();
			handling_signal = false;
		}

		bool signal_pending()
		{
			bool ret = _signal_pending;
			_signal_pending = false;
			return ret;
		}

		template <typename... ARGS>
		Task(Genode::Entrypoint & ep, ARGS &&... args)
		: task(args...), handler(ep, *this, &Task::handle_signal) {}
	};

	struct Device
	{
		using Le = Genode::List_element<Device>;

		Le                             le { this };
		Label                          label;
		Driver                        &driver;
		Genode::Env                   &env;

		/*
		 * Dedicated allocator per device to notice dangling
		 * allocations on device destruction.
		 */
		Genode::Allocator_avl          alloc;

		Task                           state_task;
		Task                           urb_task;

		Usb::Connection                usb { env, &alloc, label.string(),
		                                     512 * 1024, state_task.handler };
		usb_device                   * udev = nullptr;
		bool                           updated = true;

		Device(Driver & drv, Label label);
		~Device();

		static void state_task_entry(void *);
		static void urb_task_entry(void *);
		void register_device();
		void unregister_device();
		void probe_interface(usb_interface *, usb_device_id *);
		void remove_interface(usb_interface *);
		bool deinit();
	};

	struct Devices : Genode::List<Device::Le>
	{
		template <typename FN>
		void for_each(FN const & fn)
		{
			Device::Le * cur = first();
			for (Device::Le * next = cur ? cur->next() : nullptr; cur;
			     cur = next, next = cur ? cur->next() : nullptr)
				fn(*cur->object());
		}
	};

	enum Input_event {
		EVENT_TYPE_PRESS,
		EVENT_TYPE_RELEASE,
		EVENT_TYPE_MOTION,
		EVENT_TYPE_WHEEL,
		EVENT_TYPE_TOUCH
	};

	Devices                         devices;
	Genode::Env                    &env;
	Genode::Entrypoint             &ep    { env.ep() };
	Genode::Heap                    heap  { env.ram(), env.rm() };
	Event::Connection               event { env };
	Genode::Constructible<Task>     main_task;
	Genode::Constructible<Genode::Attached_rom_dataspace> report_rom;

	Driver(Genode::Env &env);

	void scan_report();

	static unsigned long screen_x;
	static unsigned long screen_y;
	static bool          multi_touch;

	static void main_task_entry(void *);
	static void input_callback(Input_event type, unsigned code,
	                           int ax, int ay, int rx, int ry);
};

#endif /* _SRC__DRIVERS__USB_HID__DRIVER_H_ */
