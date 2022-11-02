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

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <usb_session/connection.h>
#include <util/reconstructible.h>

/* local includes */
#include <uplink_client.h>

/* Linux emulation environment includes */
#include <legacy/lx_kit/scheduler.h>
#include <lx_emul.h>
#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>

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


	class Sync_packet : public Usb::Completion
	{
		private:

			Usb::Session_client  & _usb;
			Usb::Packet_descriptor _packet { _usb.alloc_packet(0) };
			completion             _comp;

		public:

			Sync_packet(Usb::Session_client &usb) : _usb(usb)
			{
				init_completion(&_comp);
			}

			virtual ~Sync_packet()
			{
				_usb.source()->release_packet(_packet);
			}

			void complete(Usb::Packet_descriptor &p) override
			{
				::complete(&_comp);
			}

			void send()
			{
				_packet.completion = this;
				_usb.source()->submit_packet(_packet);
				wait_for_completion(&_comp);
			}

			void config(int configuration)
			{
				_packet.type   = Usb::Packet_descriptor::CONFIG;
				_packet.number = configuration;
				send();
			}

			void alt_setting(int interface, int alt_setting)
			{
				_packet.type = Usb::Packet_descriptor::ALT_SETTING;
				_packet.interface.number      = interface;
				_packet.interface.alt_setting = alt_setting;
				send();
			}
	};

	Devices                                                devices;
	Genode::Env                                           &env;
	Genode::Entrypoint                                    &ep             { env.ep() };
	Genode::Attached_rom_dataspace                         config_rom     { env, "config" };
	Genode::Heap                                           heap           { env.ram(), env.rm() };
	Genode::Allocator_avl                                  alloc          { &heap };
	Genode::Constructible<Genode::Uplink_client>           uplink_client  { };
	Genode::Constructible<Task>                            main_task;
	Genode::Constructible<Genode::Attached_rom_dataspace>  report_rom;

	Driver(Genode::Env &env);

	void activate_network_session()
	{
		uplink_client.construct(
			env, heap,
			config_rom.xml().attribute_value(
				"uplink_label", Genode::Session_label::String { "" }));
	}

	static void main_task_entry(void *);
};

#endif /* _SRC__DRIVERS__USB_HID__DRIVER_H_ */
