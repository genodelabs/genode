/*
 * \brief  USB net driver
 * \author Stefan Kalkowski
 * \date   2018-06-07
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/component.h>

#include <driver.h>
#include <lx_emul.h>

#include <legacy/lx_kit/env.h>
#include <legacy/lx_kit/malloc.h>
#include <legacy/lx_kit/scheduler.h>
#include <legacy/lx_kit/timer.h>
#include <legacy/lx_kit/work.h>

#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>

struct workqueue_struct *tasklet_wq;


void Driver::Device::scan_altsettings(usb_interface * iface,
                                      unsigned iface_idx, unsigned alt_idx)
{
	Usb::Interface_descriptor iface_desc;
	usb.interface_descriptor(iface_idx, alt_idx, &iface_desc);
	Genode::memcpy(&iface->altsetting[alt_idx].desc, &iface_desc,
	               sizeof(usb_interface_descriptor));
	if (iface_desc.active)
		iface->cur_altsetting = &iface->altsetting[alt_idx];

	Usb::Interface_extra iface_extra;
	if (usb.interface_extra(iface_idx, alt_idx, &iface_extra)) {
		iface->altsetting[alt_idx].extra = (unsigned char *)kzalloc(iface_extra.length, 0);
		Genode::memcpy(iface->altsetting[alt_idx].extra, iface_extra.data,
		               iface_extra.length);
		iface->altsetting[alt_idx].extralen = iface_extra.length;
	}

	iface->altsetting[alt_idx].endpoint = (usb_host_endpoint*)
		kzalloc(sizeof(usb_host_endpoint)*iface->altsetting[alt_idx].desc.bNumEndpoints, GFP_KERNEL);

	for (unsigned i = 0; i < iface->altsetting[alt_idx].desc.bNumEndpoints; i++) {
		Usb::Endpoint_descriptor ep_desc[7];
		usb.endpoint_descriptor(iface_idx, alt_idx, i, ep_desc);
		Genode::memcpy(&iface->altsetting[alt_idx].endpoint[i].desc,
		               ep_desc, sizeof(usb_endpoint_descriptor));
		int epnum = usb_endpoint_num(&iface->altsetting[alt_idx].endpoint[i].desc);
		if (usb_endpoint_dir_out(&iface->altsetting[alt_idx].endpoint[i].desc))
			udev->ep_out[epnum] = &iface->altsetting[alt_idx].endpoint[i];
		else
			udev->ep_in[epnum]  = &iface->altsetting[alt_idx].endpoint[i];
	}
}


void Driver::Device::scan_interfaces(unsigned iface_idx)
{
	struct usb_interface * iface =
		(usb_interface*) kzalloc(sizeof(usb_interface), GFP_KERNEL);
	iface->num_altsetting = usb.alt_settings(iface_idx);
	iface->altsetting     = (usb_host_interface*)
		kzalloc(sizeof(usb_host_interface)*iface->num_altsetting, GFP_KERNEL);
	iface->dev.parent = &udev->dev;
	iface->dev.bus    = (bus_type*) 0xdeadbeef;

	for (unsigned i = 0; i < iface->num_altsetting; i++)
		scan_altsettings(iface, iface_idx, i);

	udev->config->interface[iface_idx] = iface;
};


void Driver::Device::register_device()
{
	if (udev) {
		Genode::error("device already registered!");
		return;
	}

	Usb::Device_descriptor dev_desc;
	Usb::Config_descriptor config_desc;
	usb.config_descriptor(&dev_desc, &config_desc);

	udev = (usb_device*) kzalloc(sizeof(usb_device), GFP_KERNEL);
	udev->bus = (usb_bus*) kzalloc(sizeof(usb_bus), GFP_KERNEL);
	udev->config = (usb_host_config*) kzalloc(sizeof(usb_host_config), GFP_KERNEL);
	udev->bus->bus_name = "usbbus";
	udev->bus->controller = (device*) (&usb);

	udev->descriptor.idVendor  = dev_desc.vendor_id;
	udev->descriptor.idProduct = dev_desc.product_id;
	udev->descriptor.bcdDevice = dev_desc.device_release;

	for (unsigned i = 0; i < config_desc.num_interfaces; i++)
		scan_interfaces(i);

	udev->actconfig = udev->config;
	udev->config->desc.bNumInterfaces = config_desc.num_interfaces;

	/* probe */
	for (unsigned i = 0; i < config_desc.num_interfaces; i++) {
		struct usb_device_id id;
		probe_interface(udev->config->interface[i], &id);
	}

	driver.activate_network_session();
}


void Driver::Device::unregister_device()
{
	for (unsigned i = 0; i < USB_MAXINTERFACES; i++) {
		if (!udev->config->interface[i]) break;
		else remove_interface(udev->config->interface[i]);
	}
	kfree(udev->bus);
	kfree(udev->config);
	kfree(udev);
	udev = nullptr;
}


void Driver::Device::state_task_entry(void * arg)
{
	Device & dev = *reinterpret_cast<Device*>(arg);

	for (;;) {
		if (dev.usb.plugged() && !dev.udev)
			dev.register_device();

		if (!dev.usb.plugged() && dev.udev)
			dev.unregister_device();

		Lx::scheduler().current()->block_and_schedule();
	}
}


void Driver::Device::urb_task_entry(void * arg)
{
	Device & dev = *reinterpret_cast<Device*>(arg);

	for (;;) {
		while (dev.udev && dev.usb.source()->ack_avail()) {
			Usb::Packet_descriptor p = dev.usb.source()->get_acked_packet();
			if (p.completion) p.completion->complete(p);
			dev.usb.source()->release_packet(p);
		}

		Lx::scheduler().current()->block_and_schedule();
	}
}


Driver::Device::Device(Driver & driver, Label label)
: label(label),
  driver(driver),
  env(driver.env),
  alloc(driver.alloc),
  state_task(env.ep(), state_task_entry, reinterpret_cast<void*>(this),
             "usb_state", Lx::Task::PRIORITY_0, Lx::scheduler()),
  urb_task(env.ep(), urb_task_entry, reinterpret_cast<void*>(this),
             "usb_urb", Lx::Task::PRIORITY_0, Lx::scheduler())
{
	usb.tx_channel()->sigh_ack_avail(urb_task.handler);
	driver.devices.insert(&le);
}


Driver::Device::~Device()
{
	driver.devices.remove(&le);
	if (udev) unregister_device();

	while (usb.source()->ack_avail()) {
		Usb::Packet_descriptor p = usb.source()->get_acked_packet();
		usb.source()->release_packet(p);
	}
}


void Driver::main_task_entry(void * arg)
{
	Driver * driver = reinterpret_cast<Driver*>(arg);

	tasklet_wq = alloc_workqueue("tasklet_wq", 0, 0);

	skb_init();
	module_usbnet_init();
	module_smsc95xx_driver_init();
	module_asix_driver_init();
	module_ax88179_178a_driver_init();
	module_cdc_driver_init();
	module_rndis_driver_init();

	static Device dev(*driver, Label(""));

	for (;;) Lx::scheduler().current()->block_and_schedule();
}


Driver::Driver(Genode::Env &env) : env(env)
{
	Genode::log("--- USB net driver ---");

	Lx_kit::construct_env(env);
	Lx::scheduler(&env);
	Lx::malloc_init(env, heap);
	Lx::timer(&env, &ep, &heap, &jiffies);
	Lx::Work::work_queue(&heap);

	main_task.construct(env.ep(), main_task_entry, reinterpret_cast<void*>(this),
	                    "main", Lx::Task::PRIORITY_0, Lx::scheduler());

	/* give all task a first kick before returning */
	Lx::scheduler().schedule();
}


void Component::construct(Genode::Env &env)
{
	env.exec_static_constructors();
	static Driver driver(env);
}
