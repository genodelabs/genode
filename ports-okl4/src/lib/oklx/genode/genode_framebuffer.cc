/*
 * \brief  Genode C API framebuffer functions of the OKLinux support library
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <dataspace/client.h>
#include <dataspace/client.h>
#include <framebuffer_session/connection.h>
#include <nitpicker_view/client.h>

/* Oklx library includes */
#include <oklx_memory_maps.h>
#include <oklx_screens.h>


Screen_array* Screen_array::screens()
{
	static Screen_array _screens;
	return &_screens;
}


extern "C" {

	namespace Okl4 {
#include <genode/framebuffer.h>
	}

	using namespace Okl4;


	int genode_screen_count()
	{
		return Screen_array::screens()->count();
	}


	unsigned long genode_fb_size(unsigned screen)
	{
		Screen *s = Screen_array::screens()->get(screen);
		if (s && s->framebuffer()) {
			Genode::Dataspace_client dsc(s->framebuffer()->dataspace());
			return dsc.size();
		}
		return 0;
	}


	void *genode_fb_attach(unsigned screen)
	{
		using namespace Genode;

		Screen *s = Screen_array::screens()->get(screen);
		if (!s || !s->framebuffer())
			return (void*) 0;

		/* Attach the framebuffer */
		Dataspace_capability cap = s->framebuffer()->dataspace();
		Dataspace_client dsc(cap);
		void* base = Genode::env()->rm_session()->attach(cap);
		size_t size = dsc.size();

		/* Put the framebuffer area in our database*/
		Memory_area::memory_map()->insert(new (env()->heap())
		                                  Memory_area((addr_t)base, size, cap));
		return (void*)base;
	}


	void genode_fb_info(unsigned screen, int *out_w, int *out_h)
	{
		Screen *s = Screen_array::screens()->get(screen);
		if (s && s->framebuffer()) {
			Framebuffer::Mode const mode = s->framebuffer()->mode();
			*out_w = mode.width();
			*out_h = mode.height();
		}
	}


	void genode_fb_refresh(unsigned screen, int x, int y, int w, int h)
	{
		Screen *s = Screen_array::screens()->get(screen);
		if (s && s->framebuffer())
			s->framebuffer()->refresh(x,y,w,h);
	}


	void genode_fb_close(unsigned screen)
	{
		Screen *s = Screen_array::screens()->get(screen);
		if (s)
			destroy(Genode::env()->heap(), s);
	}


	void genode_nit_view_create(unsigned screen, unsigned view)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if (s)
			s->put_view(view, s->nitpicker()->create_view());
	}


	void genode_nit_view_destroy(unsigned screen, unsigned view)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if (s) {
			s->nitpicker()->destroy_view(s->get_view(view));
			s->put_view(view, Nitpicker::View_capability());
		}
	}


	void genode_nit_view_back(unsigned screen, unsigned view)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if (s)
			s->nitpicker()->background(s->get_view(view));
	}

	void genode_nit_view_place(unsigned screen, unsigned view,
	                           int x, int y, int w, int h)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if (s) {
			Nitpicker::View_client v(s->get_view(view));
			v.viewport(x, y, w, h, -x, -y, false);
		}
	}

	void genode_nit_view_stack(unsigned screen, unsigned view,
	                           unsigned neighbor, int behind)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if (s) {
			Nitpicker::View_client v(s->get_view(view));
			v.stack(s->get_view(neighbor), behind, false);
		}
	}


	void genode_nit_close_all_views(unsigned screen)
	{
		Nitpicker_screen *s =
			dynamic_cast<Nitpicker_screen*>(Screen_array::screens()->get(screen));
		if(s) {
			for (unsigned i = 0; i < Nitpicker_screen::VIEW_CNT; i++) {
				Nitpicker::View_capability n = s->get_view(i);
				if (n.valid()) {
					s->nitpicker()->destroy_view(n);
					s->put_view(i, Nitpicker::View_capability());
				}
			}
		}
	}

} // extern "C"
