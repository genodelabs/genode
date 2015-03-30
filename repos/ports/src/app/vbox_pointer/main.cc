/*
 * \brief  Nitpicker pointer with support for VirtualBox-defined shapes
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <os/attached_dataspace.h>
#include <os/attached_ram_dataspace.h>
#include <os/attached_rom_dataspace.h>
#include <os/surface.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_rgb888.h>
#include <os/texture_rgb888.h>
#include <os/texture.h>
#include <util/xml_node.h>
#include <nitpicker_session/connection.h>
#include <vbox_pointer/dither_painter.h>
#include <vbox_pointer/shape_report.h>

/* local includes */
#include "big_mouse.h"

/* exception */
struct Pointer_shape_too_large { };

template <typename PT>
void convert_default_cursor_data_to_pixels(PT *pixel, Nitpicker::Area size)
{
	unsigned char *alpha = (unsigned char *)(pixel + size.count());

	for (unsigned y = 0; y < size.h(); y++) {
		for (unsigned x = 0; x < size.w(); x++) {

			/* the source is known to be in RGB565 format */
			Genode::Pixel_rgb565 src =
				*(Genode::Pixel_rgb565 *)(&big_mouse.pixels[y][x]);

			unsigned const i = y*size.w() + x;
			pixel[i] = PT(src.r(), src.g(), src.b());
			alpha[i] = src.r() ? 255 : 0;
		}
	}
}

template <typename PT>
void convert_vbox_cursor_data_to_pixels(PT *pixel, unsigned char *shape,
                                        Nitpicker::Area size)
{
	Genode::Attached_ram_dataspace texture_pixel_ds { Genode::env()->ram_session(),
	                                                  size.count() *
	                                                  sizeof(Genode::Pixel_rgb888) };

	Genode::Attached_ram_dataspace texture_alpha_ds { Genode::env()->ram_session(),
	                                                  size.count() };

	Genode::Texture<Genode::Pixel_rgb888>
		texture(texture_pixel_ds.local_addr<Genode::Pixel_rgb888>(),
		        texture_alpha_ds.local_addr<unsigned char>(),
		        size);

	for (unsigned int y = 0; y < size.h(); y++) {

		/* convert the shape data from BGRA encoding to RGBA encoding */
		unsigned char *bgra_line = &shape[y * size.w() * 4];
		unsigned char rgba_line[size.w() * 4];
		for (unsigned int i = 0; i < size.w() * 4; i += 4) {
			rgba_line[i + 0] = bgra_line[i + 2];
			rgba_line[i + 1] = bgra_line[i + 1];
			rgba_line[i + 2] = bgra_line[i + 0];
			rgba_line[i + 3] = bgra_line[i + 3];
		}

		/* import the RGBA-encoded line into the texture */
		texture.rgba(rgba_line, size.w(), y);
	}

	Genode::Pixel_alpha8 *alpha =
		reinterpret_cast<Genode::Pixel_alpha8*>(pixel + size.count());

	Genode::Surface<PT>	pixel_surface(pixel, size);
	Genode::Surface<Genode::Pixel_alpha8> alpha_surface(alpha, size);

	Dither_painter::paint(pixel_surface, texture);
	Dither_painter::paint(alpha_surface, texture);
}

//struct Log { Log(char const *msg) { PINF("Log: %s", msg); } };


typedef Genode::String<64> Domain_name;

class Domain : public Genode::List<Domain>::Element
{
	public:

		struct Name_too_long { };

	private:

		Domain_name _name;

	public:

		Domain(char const *name) : _name(name)
		{
			if (Genode::strlen(name) + 1 > _name.capacity())
				throw Name_too_long();
		}

		Domain_name const & name() { return _name; }
};


struct Domain_list : private Genode::List<Domain>
{
	void add(char const *name)
	{
		Domain *d = new (Genode::env()->heap()) Domain(name);
		insert(d);
	}

	bool contains(Domain_name const &name)
	{
		for (Domain *d = first(); d; d = d->next())
			if (d->name() == name)
				return true;

		return false;
	}
};


struct Main
{
	Genode::Attached_rom_dataspace hover_ds { "hover" };
	Genode::Attached_rom_dataspace xray_ds  { "xray" };
	Genode::Attached_rom_dataspace shape_ds { "shape" };

	Genode::Signal_receiver sig_rec;

	void handle_hover(unsigned num = 0);
	void handle_xray(unsigned num = 0);
	void handle_shape(unsigned num = 0);

	Genode::Signal_dispatcher<Main> hover_signal_dispatcher {
		sig_rec, *this, &Main::handle_hover };
	Genode::Signal_dispatcher<Main> xray_signal_dispatcher {
		sig_rec, *this, &Main::handle_xray };
	Genode::Signal_dispatcher<Main> shape_signal_dispatcher {
		sig_rec, *this, &Main::handle_shape };

	Nitpicker::Connection nitpicker;

	Nitpicker::Session::View_handle view = nitpicker.create_view();

	Domain_list vbox_domains;
	Domain_name current_domain;

	bool xray                       = false;
	bool default_pointer_visible    = false;
	bool vbox_pointer_visible       = false;
	bool vbox_pointer_shape_changed = false;

	Nitpicker::Area current_cursor_size;
	Genode::Dataspace_capability pointer_ds;

	void resize_nitpicker_buffer_if_needed(Nitpicker::Area cursor_size)
	{
		if (cursor_size != current_cursor_size) {

			Framebuffer::Mode const mode { (int)cursor_size.w(), (int)cursor_size.h(),
	                               	   	   Framebuffer::Mode::RGB565 };

			nitpicker.buffer(mode, true /* use alpha */);

			pointer_ds = nitpicker.framebuffer()->dataspace();

			current_cursor_size = cursor_size;
		}
	}

	void show_default_pointer()
	{
		if (!default_pointer_visible) {

			Nitpicker::Area const cursor_size { big_mouse.w, big_mouse.h };

			try {
				resize_nitpicker_buffer_if_needed(cursor_size);
			} catch (...) {
				PERR("%s: could not resize the pointer buffer for %u x %u pixels",
				     __func__, cursor_size.w(), cursor_size.h());
				return;
			}

			Genode::Attached_dataspace ds { pointer_ds };

			convert_default_cursor_data_to_pixels(ds.local_addr<Genode::Pixel_rgb565>(),
			                                      cursor_size);
			nitpicker.framebuffer()->refresh(0, 0, cursor_size.w(), cursor_size.h());

			Nitpicker::Rect geometry(Nitpicker::Point(0, 0), cursor_size);
			nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);
			nitpicker.execute();

			default_pointer_visible = true;
			vbox_pointer_visible    = false;
		}
	}

	void show_vbox_pointer()
	{
		if (!vbox_pointer_visible || vbox_pointer_shape_changed) {

			try {

				Vbox_pointer::Shape_report *shape_report =
					shape_ds.local_addr<Vbox_pointer::Shape_report>();

				if (shape_report->visible) {

					if ((shape_report->width == 0) || (shape_report->height == 0))
						return;

					if ((shape_report->width > Vbox_pointer::MAX_WIDTH) ||
					    (shape_report->height > Vbox_pointer::MAX_HEIGHT))
					    throw Pointer_shape_too_large();

					Nitpicker::Area const cursor_size { shape_report->width,
				                                    	shape_report->height };

					resize_nitpicker_buffer_if_needed(cursor_size);

					Genode::Attached_dataspace ds { pointer_ds };

					convert_vbox_cursor_data_to_pixels(ds.local_addr<Genode::Pixel_rgb565>(),
				                                   	   shape_report->shape,
				                                   	   cursor_size);
					nitpicker.framebuffer()->refresh(0, 0, cursor_size.w(), cursor_size.h());

					Nitpicker::Rect geometry(Nitpicker::Point(-shape_report->x_hot, -shape_report->y_hot), cursor_size);
					nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);

				} else {

					Nitpicker::Rect geometry(Nitpicker::Point(0, 0), Nitpicker::Area(0, 0));
					nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);

				}

			} catch (Genode::Volatile_object<Genode::Attached_dataspace>::Deref_unconstructed_object) {
				/* no shape has been reported, yet */ 
				Nitpicker::Rect geometry(Nitpicker::Point(0, 0), Nitpicker::Area(0, 0));
				nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);
			}

			nitpicker.execute();

			vbox_pointer_visible = true;
			vbox_pointer_shape_changed = false;
			default_pointer_visible = false;
		}
	}

	void update_pointer()
	{
		if (xray || !vbox_domains.contains(current_domain))
			show_default_pointer();
		else
			try {
				show_vbox_pointer();
			} catch (Pointer_shape_too_large) {
				PERR("%s: the pointer shape is larger than the maximum supported size of %u x %u",
				     __func__, Vbox_pointer::MAX_WIDTH, Vbox_pointer::MAX_HEIGHT);
				show_default_pointer();
			} catch (...) {
				PERR("%s: an unhandled exception occurred while trying to show \
				      the VirtualBox pointer", __func__);
				show_default_pointer();
			}
	}

	Main()
	{
		/*
		 * Try to allocate the Nitpicker buffer for the maximum supported
		 * pointer size to let the user know right from the start if the
		 * RAM quota is too low.
		 */
		Framebuffer::Mode const mode { Vbox_pointer::MAX_WIDTH, Vbox_pointer::MAX_HEIGHT,
		                               Framebuffer::Mode::RGB565 };

		nitpicker.buffer(mode, true /* use alpha */);

		/* TODO should be read from config */
		vbox_domains.add("vbox");

		/* register signal handlers */
		hover_ds.sigh(hover_signal_dispatcher);
		xray_ds.sigh(xray_signal_dispatcher);
		shape_ds.sigh(shape_signal_dispatcher);

		nitpicker.enqueue<Nitpicker::Session::Command::To_front>(view);
		nitpicker.execute();

		/* import initial state */
		handle_hover();
		handle_xray();
		handle_shape();
	}
};


static Domain_name read_string_attribute(Genode::Xml_node const &node,
                                         char             const *attr,
                                         Domain_name      const &default_value)
{
	try {
		char buf[Domain_name::capacity()];
		node.attribute(attr).value(buf, sizeof(buf));
		return Domain_name(buf);
	}
	catch (...) {
		return default_value; }
}


void Main::handle_hover(unsigned)
{
	hover_ds.update();
	if (!hover_ds.is_valid())
		return;

	/* read new hover information from nitpicker's hover report */
	try {
		Genode::Xml_node node(hover_ds.local_addr<char>());

		current_domain = read_string_attribute(node, "domain", Domain_name());
	}
	catch (...) {
		PWRN("could not parse hover report");
	}

	update_pointer();
}


void Main::handle_xray(unsigned)
{
	xray_ds.update();
	if (!xray_ds.is_valid())
		return;

	try {
		Genode::Xml_node node(xray_ds.local_addr<char>());

		xray = node.has_attribute("enabled")
		    && node.attribute("enabled").has_value("yes");
	}
	catch (...) {
		PWRN("could not parse xray report");
	}

	update_pointer();
}


void Main::handle_shape(unsigned)
{
	shape_ds.update();

	if (!shape_ds.is_valid())
		return;

	if (shape_ds.size() < sizeof(Vbox_pointer::Shape_report))
		return;

	vbox_pointer_shape_changed = true;

	update_pointer();
}


int main()
{
	static Main main;

	/* dispatch signals */
	for (;;) {

		Genode::Signal sig = main.sig_rec.wait_for_signal();
		Genode::Signal_dispatcher_base *dispatcher =
			dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}
