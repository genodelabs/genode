/*
 * \brief  Meta-data registry about the device models of Vancouver
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* local includes */
#include "device_model_registry.h"


Device_model_registry *device_model_registry()
{
	static Device_model_registry inst;
	return &inst;
}


Device_model_info::Device_model_info(char const *name, Create create,
                                     char const *arg_names[])
:
	name(name), create(create), arg_names(arg_names)
{
	device_model_registry()->insert(this);
}


/**
 * Helper macro to create global 'Device_model_info' objects
 */
#define MODEL_INFO(name, ...) \
extern "C" void __parameter_##name##_fn(Motherboard &, unsigned long *, \
                                              const char *, unsigned); \
static char const * name##_arg_names[] = { __VA_ARGS__ , 0 }; \
static Device_model_info \
	name##_model_info(#name, __parameter_##name##_fn, name##_arg_names);

#define MODEL_INFO_NO_ARG(name) MODEL_INFO(name, 0)


/*******************************
 ** Registry of device models **
 *******************************/

/*
 * For each device model, a dedicated global 'Device_model_info' object is
 * created. At construction time, each 'Device_model_info' adds itself to
 * the 'Device_model_registry'.
 *
 * We supplement the device models with the information about their argument
 * names. This enables us to describe a virtual machine via a simple XML format
 * instead of using a special syntax.
 */

MODEL_INFO(mem,     "start",     "end")
MODEL_INFO(nullio,  "io_base",   "size")
MODEL_INFO(pic,     "io_base",   "irq", "elcr_base")
MODEL_INFO(pit,     "io_base",   "irq")
MODEL_INFO(scp,     "io_port_a", "io_port_b")
MODEL_INFO(kbc,     "io_base",   "irq_kbd", "irq_aux")
MODEL_INFO(keyb,    "ps2_port",  "host_keyboard")
MODEL_INFO(mouse,   "ps2_port",  "host_mouse")
MODEL_INFO(rtc,     "io_base",   "irq")
MODEL_INFO(serial,  "io_base",   "irq", "host_serial")
MODEL_INFO(vga,     "io_base",   "fb_size")
MODEL_INFO(pmtimer, "io_port")

MODEL_INFO(pcihostbridge, "bus_num", "bus_count", "io_base", "mem_base")
MODEL_INFO(intel82576vf, "promisc", "mem_mmio", "mem_msix", "txpoll_us", "rx_map")

MODEL_INFO(ide, "port0", "port1", "irq", "bdf", "disk")
MODEL_INFO(ahci, "mem", "irq", "bdf")
MODEL_INFO(drive, "sigma0drive", "controller", "port")

MODEL_INFO(vbios_multiboot, "modaddr", "lowmem")

MODEL_INFO_NO_ARG(vbios_disk)
MODEL_INFO(vbios_keyboard, "host_keyboard")
MODEL_INFO_NO_ARG(vbios_mem)
MODEL_INFO_NO_ARG(vbios_time)
MODEL_INFO_NO_ARG(vbios_reset)
MODEL_INFO_NO_ARG(msi)
MODEL_INFO_NO_ARG(ioapic)
MODEL_INFO_NO_ARG(vcpu)
MODEL_INFO_NO_ARG(halifax)
MODEL_INFO_NO_ARG(vbios)
MODEL_INFO_NO_ARG(lapic)

MODEL_INFO(hostsink, "host_dev", "buffer")
