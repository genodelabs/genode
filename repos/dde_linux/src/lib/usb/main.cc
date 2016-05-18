/*
 * \brief  USB driver main program
 * \author Norman Feske
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \author Christian Menard <christian.menard@ksyslabs.org>
 * \date   2012-01-29
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 * Copyright (C) 2014      Ksys Labs LLC
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode */
#include <base/printf.h>
#include <base/sleep.h>
#include <os/server.h>
#include <nic_session/nic_session.h>

/* Local */
#include <platform.h>
#include <signal.h>
#include <lx_emul.h>

#include <lx_kit/irq.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/timer.h>
#include <lx_kit/work.h>


using namespace Genode;

extern "C" int subsys_usb_init();
extern "C" void subsys_input_init();
extern "C" void module_evdev_init();
extern "C" void module_hid_init();
extern "C" void module_hid_init_core();
extern "C" void module_hid_generic_init();
extern "C" void module_usb_storage_driver_init();
extern "C" void module_wacom_driver_init();
extern "C" void module_ch_driver_init();
extern "C" void module_ms_driver_init();
extern "C" void module_mt_driver_init();
extern "C" void module_raw_driver_init();

extern "C" void start_input_service(void *ep, void *services);

struct workqueue_struct *system_power_efficient_wq;
struct workqueue_struct *system_wq;
struct workqueue_struct *tasklet_wq;

void breakpoint() { PDBG("BREAK"); }

extern "C" int stdout_write(const char *);

static void run_linux(void *s)
{
	Services *services = (Services *)s;

	system_power_efficient_wq = alloc_workqueue("system_power_efficient_wq", 0, 0);
	system_wq                 = alloc_workqueue("system_wq", 0, 0);
	tasklet_wq                = alloc_workqueue("tasklet_wq", 0, 0);

	/*
	 * The RAW driver is initialized first to make sure that it doesn't miss
	 * notifications about added devices.
	 */
	if (services->raw)
		/* low level interface */
		module_raw_driver_init();

	/* USB */
	subsys_usb_init();

	/* input + HID */
	if (services->hid) {
		subsys_input_init();
		module_evdev_init();

		/* HID */
		module_hid_init_core();
		module_hid_init();
		module_hid_generic_init();
		module_ch_driver_init();
		module_ms_driver_init();
		module_mt_driver_init();
		module_wacom_driver_init();
	}

	/* storage */
	if (services->stor)
		module_usb_storage_driver_init();

	/* host controller */
	platform_hcd_init(services);

	while (true)
		Lx::scheduler().current()->block_and_schedule();
}


void start_usb_driver(Server::Entrypoint &ep)
{
	static Services services;

	if (services.hid)
		start_input_service(&ep.rpc_ep(), &services);

	Storage::init(ep);
	Nic::init(ep);

	if (services.raw)
		Raw::init(ep, services.raw_report_device_list);

	Lx::Scheduler &sched  = Lx::scheduler();
	Lx::Timer &timer = Lx::timer(&ep, &jiffies);

	Lx::Irq::irq(&ep, Genode::env()->heap());
	Lx::Work::work_queue(Genode::env()->heap());


	static Lx::Task linux(run_linux, &services, "linux", Lx::Task::PRIORITY_0,
	                      Lx::scheduler());

	Lx::scheduler().schedule();
}
