/*
 * \brief  USB net driver
 * \author Stefan Kalkowski
 * \date   2018-06-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__USB_NET__DRIVER_H_
#define _SRC__DRIVERS__USB_NET__DRIVER_H_

#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <usb_session/connection.h>
#include <lx_kit/scheduler.h>

#include <component.h>

struct usb_device_id;
struct usb_interface;
struct usb_device;

struct Driver
{
	using Label = Genode::String<64>;

	struct Task
	{
		Lx::Task                     task;
		Genode::Signal_handler<Task> handler;
	
		void handle_signal()
		{
			task.unblock();
			Lx::scheduler().schedule();
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
		Genode::Allocator_avl         &alloc;
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
		void scan_interfaces(unsigned iface_idx);
		void scan_altsettings(usb_interface * iface,
		                      unsigned iface_idx, unsigned alt_idx);
		void probe_interface(usb_interface *, usb_device_id *);
		void remove_interface(usb_interface *);
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

	Devices                         devices;
	Genode::Env                    &env;
	Genode::Entrypoint             &ep             { env.ep() };
	Genode::Heap                    heap           { env.ram(), env.rm() };
	Genode::Allocator_avl           alloc          { &heap };
	Root                            root           { env, heap };
	Genode::Constructible<Task>     main_task;
	Genode::Constructible<Genode::Attached_rom_dataspace> report_rom;

	Driver(Genode::Env &env);

	static void main_task_entry(void *);
};

#endif /* _SRC__DRIVERS__USB_HID__DRIVER_H_ */
