/*
 * \brief  Framebuffer driver quirks for FOC with USB
 * \author Martin Stein
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/mmio.h>

using namespace Genode;

void prepare_fb_driver_init(Mmio::Delayer * const d)
{
	/* FIXME: on Fiasco.OC USB HID gets stuck if HDMI starts simulatnously */
	d->usleep(1000000);
}
