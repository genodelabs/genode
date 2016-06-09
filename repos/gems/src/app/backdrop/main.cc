/*
 * \brief  Backdrop for Nitpicker
 * \author Norman Feske
 * \date   2009-08-28
 */

/*
 * Copyright (C) 2009-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <base/printf.h>
#include <util/misc_math.h>
#include <os/config.h>
#include <decorator/xml_utils.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>
#include <os/attached_dataspace.h>
#include <util/volatile_object.h>
#include <os/texture_rgb565.h>
#include <os/texture_rgb888.h>

/* gems includes */
#include <gems/png_image.h>
#include <gems/file.h>
#include <gems/xml_anchor.h>
#include <gems/texture_utils.h>

using namespace Genode;


namespace Backdrop { struct Main; }


struct Backdrop::Main
{
	Nitpicker::Connection nitpicker;

	struct Buffer
	{
		Nitpicker::Connection &nitpicker;

		/* physical screen size */
		Framebuffer::Mode const mode = nitpicker.mode();

		/**
		 * Return dataspace capability for virtual framebuffer
		 */
		Dataspace_capability _ds_cap(Nitpicker::Connection &nitpicker)
		{
			/* setup virtual framebuffer mode */
			nitpicker.buffer(mode, false);

			if (mode.format() != Framebuffer::Mode::RGB565) {
				PWRN("Color mode %d not supported\n", (int)mode.format());
				return Dataspace_capability();
			}

			return nitpicker.framebuffer()->dataspace();
		}

		Attached_dataspace fb_ds { _ds_cap(nitpicker) };

		size_t surface_num_bytes() const
		{
			return size().count()*mode.bytes_per_pixel();
		}

		Attached_ram_dataspace surface_ds { env()->ram_session(), surface_num_bytes() };

		/**
		 * Constructor
		 */
		Buffer(Nitpicker::Connection &nitpicker) : nitpicker(nitpicker) { }

		/**
		 * Return size of virtual framebuffer
		 */
		Surface_base::Area size() const
		{
			return Surface_base::Area(mode.width(), mode.height());
		}

		/**
		 * Return back buffer as painting surface
		 */
		template <typename PT>
		Surface<PT> surface()
		{
			return Surface<PT>(surface_ds.local_addr<PT>(), size());
		}

		void flush_surface()
		{
			/* blit back to front buffer */
			blit(surface_ds.local_addr<void>(), surface_num_bytes(),
			     fb_ds.local_addr<void>(), surface_num_bytes(), surface_num_bytes(), 1);
		}
	};

	Lazy_volatile_object<Buffer> buffer;

	Nitpicker::Session::View_handle view_handle = nitpicker.create_view();

	void _update_view()
	{
		/* display view behind all others */
		typedef Nitpicker::Session::Command Command;
		nitpicker.enqueue<Command::Background>(view_handle);
		Nitpicker::Rect rect(Nitpicker::Point(), buffer->size());
		nitpicker.enqueue<Command::Geometry>(view_handle, rect);
		nitpicker.enqueue<Command::To_back>(view_handle);
		nitpicker.execute();
	}

	Signal_receiver &sig_rec;

	/**
	 * Function called on config change or mode change
	 */
	void handle_config(unsigned);

	Signal_dispatcher<Main> config_dispatcher = {
		sig_rec, *this, &Main::handle_config};

	void handle_sync(unsigned);

	Signal_dispatcher<Main> sync_dispatcher = {
		sig_rec, *this, &Main::handle_sync};

	template <typename PT>
	void paint_texture(Surface<PT> &, Texture<PT> const &, Surface_base::Point, bool);

	void apply_image(Xml_node);
	void apply_fill(Xml_node);

	Main(Signal_receiver &sig_rec) : sig_rec(sig_rec)
	{
		/* trigger application of initial config */
		Signal_transmitter(config_dispatcher).submit();

		nitpicker.mode_sigh(config_dispatcher);

		config()->sigh(config_dispatcher);
	}
};


/**
 * Calculate designated image size with proportional scaling applied
 */
static Surface_base::Area calc_scaled_size(Xml_node operation,
                                           Surface_base::Area image_size,
                                           Surface_base::Area mode_size)
{
	char const *attr = "scale";
	if (!operation.has_attribute(attr))
		return image_size;

	/* prevent division by zero, below */
	if (image_size.count() == 0)
		return image_size;

	/*
	 * Determine scale ratio (in 16.16 fixpoint format)
	 */
	unsigned const ratio =
		operation.attribute(attr).has_value("fit") ?
			min((mode_size.w() << 16) / image_size.w(),
			    (mode_size.h() << 16) / image_size.h()) :
		operation.attribute(attr).has_value("zoom") ?
			max((mode_size.w() << 16) / image_size.w(),
			    (mode_size.h() << 16) / image_size.h()) :
		1 << 16;

	/*
	 * We add 0.5 (1 << 15) to round instead of truncating the fractional
	 * part when converting the fixpoint numbers to integers.
	 */
	return Surface_base::Area((image_size.w()*ratio + (1 << 15)) >> 16,
	                          (image_size.h()*ratio + (1 << 15)) >> 16);
}


template <typename PT>
void Backdrop::Main::paint_texture(Surface<PT> &surface, Texture<PT> const &texture,
                                   Surface_base::Point pos, bool tiled)
{
	/* prevent division by zero */
	if (texture.size().count() == 0)
		return;

	if (tiled) {

		/* shortcuts */
		int const w = texture.size().w(), surface_w = surface.size().w();
		int const h = texture.size().h(), surface_h = surface.size().h();

		/* draw tiles across the whole surface */
		for (int y = (pos.y() % h) - h; y < surface_h + h; y += h)
			for (int x = (pos.x() % w) - w; x < surface_w + w; x += w)
				Texture_painter::paint(surface, texture, Color(),
				                       Texture_painter::Point(x, y),
				                       Texture_painter::SOLID,
				                       true);

	} else {

		Texture_painter::paint(surface, texture, Color(), pos,
		                       Texture_painter::SOLID, true);
	}
}


void Backdrop::Main::apply_image(Xml_node operation)
{
	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;

	if (!operation.has_attribute("png")) {
		PWRN("missing 'png' attribute in <image> node");
		return;
	}

	char png_file_name[256];
	png_file_name[0] = 0;
	operation.attribute("png").value(png_file_name, sizeof(png_file_name));

	File file(png_file_name, *env()->heap());

	Anchor anchor(operation);

	Png_image png_image(file.data<void>());

	Area const scaled_size = calc_scaled_size(operation, png_image.size(),
	                                          Area(buffer->mode.width(),
	                                               buffer->mode.height()));
	/*
	 * Determine parameters of graphics operation
	 */
	int const h_gap = (int)buffer->mode.width()  - scaled_size.w(),
	          v_gap = (int)buffer->mode.height() - scaled_size.h();

	int const anchored_xpos = anchor.horizontal == Anchor::LOW    ? 0
	                        : anchor.horizontal == Anchor::CENTER ? h_gap/2
	                        : anchor.horizontal == Anchor::HIGH   ? h_gap
	                        : 0;

	int const anchored_ypos = anchor.vertical == Anchor::LOW    ? 0
	                        : anchor.vertical == Anchor::CENTER ? v_gap/2
	                        : anchor.vertical == Anchor::HIGH   ? v_gap
	                        : 0;

	Point const offset = Decorator::point_attribute(operation);

	Point const pos = Point(anchored_xpos, anchored_ypos) + offset;

	bool const tiled = operation.attribute_value("tiled", false);

	unsigned alpha = Decorator::attribute(operation, "alpha", 256U);

	/* obtain texture containing the pixels of the PNG image */
	Texture<Pixel_rgb888> *png_texture = png_image.texture<Pixel_rgb888>();

	/* create texture with the scaled image */
	Chunky_texture<Pixel_rgb888> scaled_texture(*env()->ram_session(), scaled_size);
	scale(*png_texture, scaled_texture);

	png_image.release_texture(png_texture);

	/*
	 * Code specific for the screen mode's pixel format
	 */

	/* create texture with down-sampled scaled image */
	typedef Pixel_rgb565 PT;
	Chunky_texture<PT> texture(*env()->ram_session(), scaled_size);
	convert_pixel_format(scaled_texture, texture, alpha);

	/* paint texture onto surface */
	Surface<PT> surface = buffer->surface<PT>();
	paint_texture(surface, texture, pos, tiled);
}


void Backdrop::Main::apply_fill(Xml_node operation)
{
	/*
	 * Code specific for the screen mode's pixel format
	 */

	/* create texture with down-sampled scaled image */
	typedef Pixel_rgb565 PT;

	Surface<PT> surface = buffer->surface<PT>();

	Color const color = Decorator::attribute(operation, "color", Color(0, 0, 0));

	Box_painter::paint(surface, Surface_base::Rect(Surface_base::Point(0, 0),
	                                               buffer->size()), color);
}


void Backdrop::Main::handle_config(unsigned)
{
	config()->reload();

	buffer.construct(nitpicker);

	/* clear surface */
	apply_fill(Xml_node("<fill color=\"#000000\"/>"));

	/* apply graphics primitives defined in the config */
	try {
		for (unsigned i = 0; i < config()->xml_node().num_sub_nodes(); i++) {
			try {
				Xml_node operation = config()->xml_node().sub_node(i);

				if (operation.has_type("image"))
					apply_image(operation);

				if (operation.has_type("fill"))
					apply_fill(operation);
			}
			catch (...) {
				/*
				 * Ignore failure of individual operation, i.e., non-existing
				 * files or malformed PNG data.
				 */
			}
		}
	} catch (...) { /* ignore failure to obtain config */ }

	/* schedule buffer refresh */
	nitpicker.framebuffer()->sync_sigh(sync_dispatcher);
}


void Backdrop::Main::handle_sync(unsigned)
{
	buffer->flush_surface();
	_update_view();

	/* disable sync signal until the next call of 'handle_config' */
	nitpicker.framebuffer()->sync_sigh(Signal_context_capability());
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }


int main(int argc, char **argv)
{
	static Signal_receiver sig_rec;

	static Backdrop::Main application(sig_rec);

	/* process incoming signals */
	for (;;) {
		using namespace Genode;

		Signal sig = sig_rec.wait_for_signal();
		Signal_dispatcher_base *dispatcher =
			dynamic_cast<Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}
