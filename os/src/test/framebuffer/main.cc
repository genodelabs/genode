/*
 * \brief  Basic test for framebuffer session
 * \author Martin Stein
 * \date   2012-01-09
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/env.h>

using namespace Genode;

int main()
{
	printf("--- Test framebuffer ---\n");

	/* create framebuffer */
	static Framebuffer::Connection fb;
	Framebuffer::Mode const mode = fb.mode();
	PINF("framebuffer is %dx%d@%d\n", mode.width(), mode.height(), mode.format());
	Dataspace_capability fb_ds_cap = fb.dataspace();
	if (!fb_ds_cap.valid()) {
		PERR("Could not request dataspace for frame buffer");
		return -2;
	}
	Framebuffer::Mode const fb_mode = fb.mode();

	/* write pixeldata to framebuffer */
	void * fb_base = env()->rm_session()->attach(fb_ds_cap);
	unsigned j;
	unsigned const fb_size = (unsigned)(mode.width()*mode.height())/2;
	for(unsigned i = 0; i < fb_size; i++)
	{
		*(((unsigned volatile *)fb_base) + i) = j;
		j++;
	}
	fb.refresh(0, 0, fb_mode.width(), fb_mode.height());
	printf("--- end ---\n");
	while(1);
	return 0;
}

