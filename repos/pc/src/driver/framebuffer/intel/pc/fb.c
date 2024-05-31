/*
 * \brief  Linux kernel framebuffer device support
 * \author Stefan Kalkowski
 * \date   2021-05-03
 */

/*
 * Copyright (C) 2021-2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <lx_emul/fb.h>


int register_framebuffer(struct fb_info * fb_info)
{
	lx_emul_framebuffer_ready(fb_info->screen_base, fb_info->screen_size,
	                          fb_info->var.xres_virtual, fb_info->var.yres_virtual,
	                          fb_info->fix.line_length /
	                          (fb_info->var.bits_per_pixel / 8), fb_info->var.yres);
	return 0;
}
