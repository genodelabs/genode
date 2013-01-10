/*
 * \brief  Oklx's framebuffer driver for Genode ioctl interface
 * \author Stefan Kalkowski
 * \date   2010-04-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLX_KERNEL__OKLX__IOCTL_H_
#define _INCLUDE__OKLX_KERNEL__OKLX__IOCTL_H_

struct genode_screen_region
{
	int x;
	int y;
	int w;
	int h;
};

struct genode_view_place
{
	int view;
	struct genode_screen_region reg;
};

struct genode_view_stack
{
	int view;
	int neighbor;
	int behind;
};

enum Ioctl_nums {
	NITPICKER_IOCTL_CREATE_VIEW  = _IOW('F', 11, int),
	NITPICKER_IOCTL_DESTROY_VIEW = _IOW('F', 12, int),
	NITPICKER_IOCTL_BACK_VIEW    = _IOW('F', 13, int),
	NITPICKER_IOCTL_PLACE_VIEW   = _IOW('F', 14, struct genode_view_place),
	NITPICKER_IOCTL_STACK_VIEW   = _IOW('F', 15, struct genode_view_stack),
	FRAMEBUFFER_IOCTL_REFRESH    = _IOW('F', 16, struct genode_screen_region)
};

#endif //_INCLUDE__OKLX_KERNEL__OKLX__IOCTL_H_
