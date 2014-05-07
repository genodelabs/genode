/*
 * \brief  Genode screen driver
 * \author Stefan Kalkowski <kalkowski@genode-labs.com>
 * \date   2010-04-20
 *
 * This driver enables usage of any of Genode's framebuffer, input
 * and nitpicker sessions, as defined in Linux corresponding XML config stub.
 * The implementation is based on virtual (vfb.c) and
 * L4 (l4fb.c) framebuffer driver of L4Linux from TU-Dresden.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/screen_info.h>
#include <linux/sched.h>

/* Platform includes */
#include <asm/uaccess.h>

/* L4 includes */
#include <l4/util/util.h>

/* Genode support lib includes */
#include <genode/framebuffer.h>
#include <genode/input.h>


/**********************************
 **  Datastructure declarations  **
 **********************************/

/**
 * List of available framebuffers (used by device->driver_data)
 */
struct genodefb_infolist {
	struct fb_info           *info;
	struct genodefb_infolist *next;
};


/**********************
 ** Global variables **
 **********************/

static const int  IRQ_KEYBOARD = 2;

static const char GENODEFB_DRV_NAME[] = "genodefb";

static unsigned int      poll_sleep = HZ / 10;
static struct timer_list input_timer;

static struct fb_var_screeninfo genodefb_var __initdata = {
	.activate       = FB_ACTIVATE_NOW,
	.height         = -1,
	.width          = -1,
	.right_margin   = 32,
	.upper_margin   = 16,
	.lower_margin   = 4,
	.vsync_len      = 4,
	.vmode          = FB_VMODE_NONINTERLACED,
	.bits_per_pixel = 16, // Genode only supports RGB565 by now */
	.red.length     = 5,
	.red.offset     = 11,
	.green.length   = 6,
	.green.offset   = 5,
	.blue.length    = 5,
	.blue.offset    = 0,
	.transp.length  = 0,
	.transp.offset  = 0,
};

static struct fb_fix_screeninfo genodefb_fix __initdata = {
	.id        = "genode_fb",
	.type      = FB_TYPE_PACKED_PIXELS,
	.accel     = FB_ACCEL_NONE,
	.visual    = FB_VISUAL_TRUECOLOR,
	.ypanstep  = 0,
	.ywrapstep = 0,
};

static u32 pseudo_palette[17];


/*************************
 **  Device operations  **
 *************************/

/*
 *  Set a single color register. The values supplied are
 *  already rounded down to the hardware's capabilities
 *  (according to the entries in the `var' structure). Return
 *  != 0 for invalid regno and pixel formats.
 */
static int genodefb_setcolreg(unsigned regno, unsigned red, unsigned green,
                              unsigned blue, unsigned transp,
                              struct fb_info *info)
{
	if (regno >= info->cmap.len || info->var.bits_per_pixel != 16)
		return 1;

	if (regno < 16)
		((u32*) (info->pseudo_palette))[regno] =
			((red   >> (16 -   info->var.red.length)) <<   info->var.red.offset) |
			((green >> (16 - info->var.green.length)) << info->var.green.offset) |
			((blue  >> (16 -  info->var.blue.length)) <<  info->var.blue.offset);
	return 0;
}


/**
 *  Pan or Wrap the Display
 *
 *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
 */
static int genodefb_pan_display(struct fb_var_screeninfo *var,
                                struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0
		    || var->yoffset >= info->var.yres_virtual
		    || var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset + var->xres > info->var.xres_virtual ||
		    var->yoffset + var->yres > info->var.yres_virtual)
			return -EINVAL;
	}
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}


static void genodefb_copyarea(struct fb_info *info,
                              const struct fb_copyarea *region)
{
	cfb_copyarea(info, region);
	genode_fb_refresh(info->node, region->dx, region->dy,
	                  region->width, region->height);
}


static void genodefb_fillrect(struct fb_info *info,
                              const struct fb_fillrect *rect)
{
	cfb_fillrect(info, rect);
	genode_fb_refresh(info->node, rect->dx, rect->dy,
	                  rect->width, rect->height);
}


static void genodefb_imageblit(struct fb_info *info,
                               const struct fb_image *image)
{
	cfb_imageblit(info, image);
	genode_fb_refresh(info->node, image->dx, image->dy,
	                  image->width, image->height);
}


static int genodefb_mmap(struct fb_info *info,
                         struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;

	if (offset + size > info->fix.smem_len)
		return -EINVAL;

	pfn = ((unsigned long)info->fix.smem_start + offset) >> PAGE_SHIFT;
	while (size > 0) {
		if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, PAGE_SHARED)) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pfn++;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	l4_touch_rw((char *)info->fix.smem_start + offset,
	            vma->vm_end - vma->vm_start);

	return 0;
}


static int genodefb_open(struct fb_info *info, int user)
{
	return 0;
}


static int genodefb_release(struct fb_info *info, int user)
{
	return 0;
}


static struct fb_ops genodefb_ops = {
	.owner          = THIS_MODULE,
	.fb_open        = genodefb_open,
	.fb_release     = genodefb_release,
	.fb_setcolreg   = genodefb_setcolreg,
	.fb_pan_display = genodefb_pan_display,
	.fb_fillrect    = genodefb_fillrect,
	.fb_copyarea    = genodefb_copyarea,
	.fb_imageblit   = genodefb_imageblit,
	.fb_mmap        = genodefb_mmap,
};


/***********************
 **  Input callbacks  **
 ***********************/

void FASTCALL
input_event_callback (void *dev, unsigned int type,
                      unsigned int code, int value)
{
	struct input_dev *input_dev = (struct input_dev*) dev;

#ifdef CONFIG_ANDROID
	if (type == EV_KEY && code == BTN_LEFT)
		code = BTN_TOUCH;
#endif

	input_event(input_dev, type, code, value);
	input_sync(input_dev);
}



static void genodefb_poll_for_events(unsigned long data)
{
	genode_input_handle_events();
	mod_timer(&input_timer, jiffies + poll_sleep);
}


/***************************************
 **  Device initialization / removal  **
 ***************************************/

static int __init genodefb_register_input_devices(unsigned int idx,
                                                  unsigned int xres,
                                                  unsigned int yres)
{
	int i;
	struct input_dev *mouse_dev = input_allocate_device();
	struct input_dev *keyb_dev  = input_allocate_device();
	if (!keyb_dev || !mouse_dev)
		return -ENOMEM;


	/****************
	 **  Keyboard  **
	 ****************/

	keyb_dev->name       = "Genode input key";
	keyb_dev->phys       = "Genode fb key";
	keyb_dev->id.bustype = BUS_USB;
	keyb_dev->id.vendor  = 0;
	keyb_dev->id.product = 0;
	keyb_dev->id.version = 0;

	/* We generate key events */
	set_bit(EV_KEY, keyb_dev->evbit);
	set_bit(EV_REP, keyb_dev->evbit);

	/* We can generate every key */
	for (i = 0; i < 0x100; i++)
		set_bit(i, keyb_dev->keybit);

	/* Register keyboard device */
	if (input_register_device(keyb_dev)) {
		input_free_device(keyb_dev);
		printk(KERN_WARNING "cannot register keyboard!");
		return -1;
	}
	genode_input_register_keyb(idx, (void*) keyb_dev);


	/*************
	 **  Mouse  **
	 *************/

	mouse_dev->name = "Genode input mouse";
	mouse_dev->phys = "Genode mouse";
	mouse_dev->id.bustype = BUS_USB;
	mouse_dev->id.vendor  = 0;
	mouse_dev->id.product = 0;
	mouse_dev->id.version = 0;

	/* We generate key and relative mouse events */
	set_bit(EV_KEY, mouse_dev->evbit);
	set_bit(EV_REP, mouse_dev->evbit);
#ifndef CONFIG_ANDROID
	set_bit(EV_REL, mouse_dev->evbit);
#endif
	set_bit(EV_ABS, mouse_dev->evbit);
	set_bit(EV_SYN, mouse_dev->evbit);

	/* Buttons */
#ifdef CONFIG_ANDROID
	set_bit(BTN_TOUCH,  mouse_dev->keybit);
#else
	set_bit(BTN_0,      mouse_dev->keybit);
	set_bit(BTN_1,      mouse_dev->keybit);
	set_bit(BTN_2,      mouse_dev->keybit);
	set_bit(BTN_3,      mouse_dev->keybit);
	set_bit(BTN_4,      mouse_dev->keybit);
	set_bit(BTN_LEFT,   mouse_dev->keybit);
	set_bit(BTN_RIGHT,  mouse_dev->keybit);
	set_bit(BTN_MIDDLE, mouse_dev->keybit);
#endif

	/* Movements */
#ifndef CONFIG_ANDROID
	set_bit(REL_X, mouse_dev->relbit);
	set_bit(REL_Y, mouse_dev->relbit);
#endif
	set_bit(ABS_X, mouse_dev->absbit);
	set_bit(ABS_Y, mouse_dev->absbit);

	input_set_abs_params(mouse_dev, ABS_PRESSURE, 0, 1, 0, 0);

	/* Coordinates are 1:1 pixel in frame buffer */
	input_set_abs_params(mouse_dev, ABS_X, 0, xres, 0, 0);
	input_set_abs_params(mouse_dev, ABS_Y, 0, yres, 0, 0);

	/* Register mouse device */
	if (input_register_device(mouse_dev)) {
		input_free_device(mouse_dev);
		printk(KERN_WARNING "cannot register mouse!");
		return -1;
	}
	genode_input_register_mouse(idx, (void*) mouse_dev);

	init_timer(&input_timer);
	input_timer.function = genodefb_poll_for_events;
	input_timer.expires  = jiffies + poll_sleep;
	add_timer(&input_timer);
	return 0;
}


static int __init genodefb_probe(struct platform_device *dev)
{
	struct genodefb_infolist *pred=0, *succ;
	int i, ret, cnt = genode_screen_count();

	/*
	 * Iterate through all available framebuffers
	 */
	for (i=0; i < cnt; i++) {

		/* Allocate new framebuffer list entry */
		if(!(succ = kmalloc(sizeof(struct genodefb_infolist), GFP_KERNEL)))
			return -ENOMEM;
		succ->next = 0;

		/* Set first entry as driver's private data, else concatenate it */
		if (!i)
			platform_set_drvdata(dev, succ);
		else
			pred->next = succ;
		pred = succ;

		/* Allocate new framebuffer info struct */
		pred->info = framebuffer_alloc(0, &dev->dev);
		if (!pred->info)
			return -ENOMEM;

		/* Copy default values */
		pred->info->var = genodefb_var;
		pred->info->fix = genodefb_fix;

		/* Get framebuffer dimensions from Genode's support lib */
		pred->info->screen_base    = genode_fb_attach(i);
		pred->info->screen_size    = genode_fb_size(i);
		pred->info->fix.smem_start = (unsigned long) pred->info->screen_base;
		pred->info->fix.smem_len   = pred->info->screen_size;
		if (!pred->info->screen_base || !pred->info->screen_size) {
			printk(KERN_ERR "genode_fb: abort, could not be initialized.\n");
			framebuffer_release(pred->info);
			return -EIO;
		}

		/* Get framebuffer resolution from Genode's support lib */
		genode_fb_info(i, &pred->info->var.xres, &pred->info->var.yres);

		/* We only support 16-Bit Pixel, so line length is xres*2 */
		pred->info->fix.line_length  = pred->info->var.xres * 2;

		/* Set virtual resolution to visible resolution */
		pred->info->var.xres_virtual = pred->info->var.xres;
		pred->info->var.yres_virtual = pred->info->screen_size
		                                / pred->info->fix.line_length;

		/* Some dummy values for timing to make fbset happy */
		pred->info->var.pixclock     = 10000000 / pred->info->var.xres
		                               * 1000 / pred->info->var.yres;
		pred->info->var.left_margin  = (pred->info->var.xres / 8) & 0xf8;
		pred->info->var.hsync_len    = (pred->info->var.xres / 8) & 0xf8;

		pred->info->fbops            = &genodefb_ops;
		pred->info->pseudo_palette   = pseudo_palette;
		pred->info->flags            = FBINFO_FLAG_DEFAULT;

		printk(KERN_INFO "genode_fb:framebuffer at 0x%p, size %dk\n",
		       pred->info->screen_base, (int)(pred->info->screen_size >> 10));
		printk(KERN_INFO "genode_fb: mode is %dx%dx%d\n",
		       pred->info->var.xres, pred->info->var.yres,
		       pred->info->var.bits_per_pixel);

		/* Allocate 16-Bit colormap */
		ret = fb_alloc_cmap(&pred->info->cmap, 16, 0);
		if (ret < 0) {
			framebuffer_release(pred->info);
			return ret;
		}

		/* Register framebuffer info structure */
		if (register_framebuffer(pred->info) < 0) {
			fb_dealloc_cmap(&pred->info->cmap);
			framebuffer_release(pred->info);
			return -EINVAL;
		}

		ret = genodefb_register_input_devices(i, pred->info->var.xres,
		                                      pred->info->var.yres);
		if (ret) {
			fb_dealloc_cmap(&pred->info->cmap);
			framebuffer_release(pred->info);
			return ret;
		}
	}
	return 0;
}


static int genodefb_remove(struct platform_device *device)
{
	struct genodefb_infolist *succ = platform_get_drvdata(device);

	while (succ && succ->info) {
		struct genodefb_infolist *pred = succ;
		succ = succ->next;
		genode_fb_close(pred->info->node);
		unregister_framebuffer(pred->info);
		framebuffer_release(pred->info);
		kfree(pred);
	}
	platform_set_drvdata(device, 0);
	return 0;
}


/***************************************
 **  Module initialization / removal  **
 ***************************************/

static struct platform_driver __refdata genodefb_driver = {
	.probe       = genodefb_probe,
	.remove      = genodefb_remove,
	.driver.name = GENODEFB_DRV_NAME,
};

static struct platform_device genodefb_device = {
	.name        = GENODEFB_DRV_NAME,
};


static int __init genodefb_init(void)
{
	int ret = platform_driver_register(&genodefb_driver);
	if (!ret) {
		ret = platform_device_register(&genodefb_device);
		if (ret)
			platform_driver_unregister(&genodefb_driver);
	}
	genode_input_register_callback(&input_event_callback);
	return ret;
}
module_init(genodefb_init);


static void __exit genodefb_exit(void)
{
	platform_device_unregister(&genodefb_device);
	platform_driver_unregister(&genodefb_driver);
	genode_input_unregister_callback();
}
module_exit(genodefb_exit);


MODULE_AUTHOR("Stefan Kalkowski <stefan.kalkowski@genode-labs.com>");
MODULE_DESCRIPTION("Frame buffer driver for Linux on Genode");
MODULE_LICENSE("GPL v2");
