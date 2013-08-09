/*
 * \brief  Drivers quirks
 * \author Sebastian Sumpf
 * \date   2012-11-27
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* OSS includes */
extern "C" {
#include <oss_config.h>
}

#include <quirks.h>

/* Genode includes */
#include <base/printf.h>


/**
 * The Qemu es1370 emulation does not support 'inb' for the serial interface
 * control register (ports  0x20 - 0x23)
 */
static unsigned char qemu_es1370_inb_quirk(_oss_device_t *osdev, addr_t port)
{
	unsigned base = osdev->res[0].base;
	unsigned addr = port - base;

	if (addr < 0x20 && addr > 0x23)
		return dde_kit_inb(port);

	unsigned val = dde_kit_inl(base + 0x20) >> (8 * (addr - 0x20));
	return val & 0xff;
}



void setup_quirks(struct oss_driver *drv)
{
	if (!strcmp(drv->name, "oss_audiopci"))
		drv->inb_quirk = qemu_es1370_inb_quirk;
}
