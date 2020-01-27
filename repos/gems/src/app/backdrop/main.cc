/*
 * \brief  Backdrop for Nitpicker
 * \author Norman Feske
 * \date   2009-08-28
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <nitpicker_session/connection.h>
#include <util/misc_math.h>
#include <decorator/xml_utils.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>
#include <base/attached_dataspace.h>
#include <util/reconstructible.h>
#include <os/texture_rgb565.h>
#include <os/texture_rgb888.h>

/* gems includes */
#include <gems/png_image.h>
#include <gems/file.h>
#include <gems/xml_anchor.h>
#include <gems/texture_utils.h>

/* libc includes */
#include <libc/component.h>

using namespace Genode;

namespace Backdrop { struct Main; }


struct Backdrop::Main
{
	Genode::Env &_env;

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	Nitpicker::Connection _nitpicker { _env, "backdrop" };

	struct Buffer
	{
		Nitpicker::Connection &nitpicker;

		Framebuffer::Mode const mode;

		/**
		 * Return dataspace capability for virtual framebuffer
		 */
		Dataspace_capability _ds_cap(Nitpicker::Connection &nitpicker)
		{
			/* setup virtual framebuffer mode */
			nitpicker.buffer(mode, false);

			if (mode.format() != Framebuffer::Mode::RGB565) {
				Genode::warning("Color mode %d not supported\n", (int)mode.format());
				return Dataspace_capability();
			}

			return nitpicker.framebuffer()->dataspace();
		}

		Attached_dataspace fb_ds;

		Genode::size_t surface_num_bytes() const
		{
			return size().count()*mode.bytes_per_pixel();
		}

		Attached_ram_dataspace surface_ds;

		/**
		 * Constructor
		 */
		Buffer(Genode::Env &env, Nitpicker::Connection &nitpicker, Framebuffer::Mode mode)
		:	nitpicker(nitpicker), mode(mode),
			fb_ds(env.rm(), _ds_cap(nitpicker)),
			surface_ds(env.ram(), env.rm(), surface_num_bytes())
		{ }

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
		template <typename PT, typename FN>
		void apply_to_surface(FN const &fn)
		{
			Surface<PT> surface(surface_ds.local_addr<PT>(), size());
			fn(surface);
		}

		void flush_surface()
		{
			/* blit back to front buffer */
			blit(surface_ds.local_addr<void>(), surface_num_bytes(),
			     fb_ds.local_addr<void>(), surface_num_bytes(), surface_num_bytes(), 1);
		}
	};

	Constructible<Buffer> _buffer { };

	Nitpicker::Session::View_handle _view_handle = _nitpicker.create_view();

	void _update_view()
	{
		/* display view behind all others */
		typedef Nitpicker::Session::Command     Command;
		typedef Nitpicker::Session::View_handle View_handle;

		_nitpicker.enqueue<Command::Background>(_view_handle);
		Nitpicker::Rect rect(Nitpicker::Point(), _buffer->size());
		_nitpicker.enqueue<Command::Geometry>(_view_handle, rect);
		_nitpicker.enqueue<Command::To_back>(_view_handle, View_handle());
		_nitpicker.execute();
	}

	/**
	 * Function called on config change or mode change
	 */
	void _handle_config();

	void _handle_config_signal() {
		Libc::with_libc([&] () { _handle_config(); }); }

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config_signal };

	void _handle_sync();

	Signal_handler<Main> _sync_handler = {
		_env.ep(), *this, &Main::_handle_sync};

	template <typename PT>
	void _paint_texture(Surface<PT> &, Texture<PT> const &, Surface_base::Point, bool);

	void _apply_image(Xml_node);
	void _apply_fill(Xml_node);

	Main(Genode::Env &env) : _env(env)
	{
		_nitpicker.mode_sigh(_config_handler);

		_config.sigh(_config_handler);

		_handle_config();
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
void Backdrop::Main::_paint_texture(Surface<PT> &surface, Texture<PT> const &texture,
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


void Backdrop::Main::_apply_image(Xml_node operation)
{
	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;

	if (!operation.has_attribute("png")) {
		Genode::warning("missing 'png' attribute in <image> node");
		return;
	}

	typedef String<256> File_name;
	File_name const png_file_name = operation.attribute_value("png", File_name());

	File file(png_file_name.string(), _heap);

	Anchor anchor(operation);

	Png_image png_image(_env.ram(), _env.rm(), _heap, file.data<void>());

	Area const scaled_size = calc_scaled_size(operation, png_image.size(),
	                                          Area(_buffer->mode.width(),
	                                               _buffer->mode.height()));
	/*
	 * Determine parameters of graphics operation
	 */
	int const h_gap = (int)_buffer->mode.width()  - scaled_size.w(),
	          v_gap = (int)_buffer->mode.height() - scaled_size.h();

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

	unsigned alpha = operation.attribute_value("alpha", 256U);

	/* obtain texture containing the pixels of the PNG image */
	Texture<Pixel_rgb888> *png_texture = png_image.texture<Pixel_rgb888>();

	/* create texture with the scaled image */
	Chunky_texture<Pixel_rgb888> scaled_texture(_env.ram(), _env.rm(), scaled_size);
	scale(*png_texture, scaled_texture, _heap);

	png_image.release_texture(png_texture);

	/*
	 * Code specific for the screen mode's pixel format
	 */

	/* create texture with down-sampled scaled image */
	typedef Pixel_rgb565 PT;
	Chunky_texture<PT> texture(_env.ram(), _env.rm(), scaled_size);
	convert_pixel_format(scaled_texture, texture, alpha, _heap);

	/* paint texture onto surface */
	_buffer->apply_to_surface<PT>([&] (Surface<PT> &surface) {
		_paint_texture(surface, texture, pos, tiled);
	});
}


void Backdrop::Main::_apply_fill(Xml_node operation)
{
	/*
	 * Code specific for the screen mode's pixel format
	 */

	/* create texture with down-sampled scaled image */
	typedef Pixel_rgb565 PT;

	Color const color = operation.attribute_value("color", Color(0, 0, 0));

	_buffer->apply_to_surface<PT>([&] (Surface<PT> &surface) {
		Box_painter::paint(surface, Surface_base::Rect(Surface_base::Point(0, 0),
		                                               _buffer->size()), color);
	});
}


void Backdrop::Main::_handle_config()
{
	_config.update();

	Framebuffer::Mode const phys_mode = _nitpicker.mode();
	Framebuffer::Mode const
		mode(_config.xml().attribute_value("width",  (unsigned)phys_mode.width()),
		     _config.xml().attribute_value("height", (unsigned)phys_mode.height()),
		     phys_mode.format());

	_buffer.construct(_env, _nitpicker, mode);

	/* clear surface */
	_apply_fill(Xml_node("<fill color=\"#000000\"/>"));

	/* apply graphics primitives defined in the config */
	_config.xml().for_each_sub_node([&] (Xml_node operation) {
		try {
			if (operation.has_type("image"))
				_apply_image(operation);

			if (operation.has_type("fill"))
				_apply_fill(operation);
		}
		catch (...) {
			/*
			 * Ignore failure of individual operation, i.e., non-existing
			 * files or malformed PNG data.
			 */
		}
	});

	/* schedule buffer refresh */
	_nitpicker.framebuffer()->sync_sigh(_sync_handler);
}


void Backdrop::Main::_handle_sync()
{
	Libc::with_libc([&] () {
		_buffer->flush_surface();
		_update_view();
	});

	/* disable sync signal until the next call of 'handle_config' */
	_nitpicker.framebuffer()->sync_sigh(Signal_context_capability());
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }


void Libc::Component::construct(Libc::Env &env) {
	with_libc([&env] () { static Backdrop::Main application(env); }); }

