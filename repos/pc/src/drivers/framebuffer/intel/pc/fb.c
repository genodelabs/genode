/*
 * \brief  Linux kernel framebuffer device support
 * \author Stefan Kalkowski
 * \date   2021-05-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <lx_emul/fb.h>

struct fb_info * framebuffer_alloc(size_t size,struct device * dev)
{
#define BYTES_PER_LONG (BITS_PER_LONG/8)
#define PADDING (BYTES_PER_LONG - (sizeof(struct fb_info) % BYTES_PER_LONG))
	int fb_info_size = sizeof(struct fb_info);
	struct fb_info *info;
	char *p;

	if (size) {
		fb_info_size += PADDING;
	}

	p = kzalloc(fb_info_size + size, GFP_KERNEL);

	if (!p)
		return NULL;

	info = (struct fb_info *) p;

	if (size)
		info->par = p + fb_info_size;

	info->device = dev;
	info->fbcon_rotate_hint = -1;

#if IS_ENABLED(CONFIG_FB_BACKLIGHT)
	mutex_init(&info->bl_curve_mutex);
#endif

	return info;
#undef PADDING
#undef BYTES_PER_LONG
}


int register_framebuffer(struct fb_info * fb_info)
{
	lx_emul_framebuffer_ready(fb_info->screen_base, fb_info->screen_size,
	                          fb_info->var.xres_virtual, fb_info->var.yres_virtual,
	                          fb_info->fix.line_length /
	                          (fb_info->var.bits_per_pixel / 8), fb_info->var.yres);
	return 0;
}
