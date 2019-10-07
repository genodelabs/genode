/*
 * \brief  Playground for painting text
 * \author Norman Feske
 * \date   2018-03-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <framebuffer_session/connection.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <base/heap.h>
#include <os/pixel_rgb565.h>
#include <os/surface.h>
#include <nitpicker_gfx/tff_font.h>
#include <nitpicker_gfx/box_painter.h>
#include <util/bezier.h>
#include <timer_session/connection.h>
#include <os/vfs.h>

/* gems includes */
#include <gems/ttf_font.h>
#include <gems/vfs_font.h>
#include <gems/cached_font.h>

namespace Test {
	using namespace Genode;

	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Rect  Rect;
	struct Main;
};


/**
 * Statically linked binary data
 */
extern char _binary_droidsansb10_tff_start[];
extern char _binary_default_tff_start[];


struct Test::Main
{
	Env &_env;

	Framebuffer::Connection _fb { _env, Framebuffer::Mode() };

	Attached_dataspace _fb_ds { _env.rm(), _fb.dataspace() };

	typedef Pixel_rgb565 PT;

	Surface_base::Area const _size { (unsigned)_fb.mode().width(),
	                                 (unsigned)_fb.mode().height() };

	Surface<PT> _surface { _fb_ds.local_addr<PT>(), _size };

	char _glyph_buffer_array[8*1024];

	Tff_font::Glyph_buffer _glyph_buffer { _glyph_buffer_array, sizeof(_glyph_buffer_array) };

	Tff_font _font_1 { _binary_droidsansb10_tff_start, _glyph_buffer };
	Tff_font _font_2 { _binary_default_tff_start,      _glyph_buffer };

	Attached_rom_dataspace _vera_ttf_ds { _env, "Vera.ttf" };

	Heap _heap { _env.ram(), _env.rm() };

	Ttf_font _font_3 { _heap, _vera_ttf_ds.local_addr<void>(), 13 };

	Attached_rom_dataspace _config { _env, "config" };

	Root_directory _root { _env, _heap, _config.xml().sub_node("vfs") };

	Vfs_font _font_4 { _heap, _root, "fonts/regular" };

	void _refresh() { _fb.refresh(0, 0, _size.w(), _size.h()); }

	Main(Env &env) : _env(env)
	{
		/* test positioning of text */
		_surface.clip(Rect(Point(0, 0), _size));
		Box_painter::paint(_surface, Rect(Point(200, 10), Area(250, 50)), Color(0, 100, 0));
		Text_painter::paint(_surface,
		                    Text_painter::Position(200, 10), _font_1,
		                    Color(255, 255, 255),
		                    "Text aligned at the top-left corner");

		Box_painter::paint(_surface, Rect(Point(200, 100), Area(250, 50)), Color(0, 100, 0));
		Text_painter::paint(_surface,
		                    Text_painter::Position(210, (int)(100 - _font_1.baseline())), _font_1,
		                    Color(255, 255, 255),
		                    "Baseline of text aligned at the top");

		/* test horizontal clipping boundaries */
		_surface.clip(Rect(Point(20, 15), Area(40, 300)));
		Box_painter::paint(_surface, Rect(Point(0, 0), _size), Color(150, 20, 10));

		for (int x = 0, y = -30; y < (int)_size.h() + 30; x++, y += _font_2.bounding_box().h())
			Text_painter::paint(_surface,
			                    Text_painter::Position(x, y), _font_2,
			                    Color(255, 255, 255),
			                    "Text painter at work");

		/* test horizontal subpixel positioning */
		_surface.clip(Rect(Point(90, 15), Area(100, 300)));
		Box_painter::paint(_surface, Rect(Point(0, 0), _size), Color(150, 20, 10));

		for (float x = 90, y = -30; y < (int)_size.h() + 30; x += 0.2, y += _font_3.bounding_box().h())
			Text_painter::paint(_surface,
			                    Text_painter::Position(x, y), _font_3,
			                    Color(255, 255, 255),
			                    "This is a real textSub-=_HT-+=%@pixel positioning");

		_surface.clip(Rect(Point(90, 320), Area(100, 300)));
		Box_painter::paint(_surface, Rect(Point(0, 0), _size), Color(255, 255, 255));

		for (float x = 90, y = 300; y < (int)_size.h() + 30; x += 0.2, y += _font_3.bounding_box().h())
			Text_painter::paint(_surface,
			                    Text_painter::Position(x, y), _font_3,
			                    Color(0, 0, 0),
			                    "This is a real textSub-=_HT-+=%@pixel positioning");
		_refresh();

		struct Lut
		{
			unsigned char value[256];

			Lut()
			{
				auto fill_segment = [&] (long x1, long y1, long x2, long)
				{
					for (long i = x1>>8; i < x2>>8; i++) value[i] = min(255, y1>>8);
				};

				bezier(0, 0, 0, 130<<8, 256<<8, 260<<8, fill_segment, 7);
			}
		};

		static Lut const lut;
		_surface.clip(Rect(Point(0, 0), _size));

		for (unsigned x = 0; x < 256; x++)
			Box_painter::paint(_surface,
			                   Rect(Point(x + 512, 280 - lut.value[x]), Area(1, 1)),
			                   Color(255, 255, 255));
		_refresh();

		_surface.clip(Rect(Point(0, 0), _size));
		char const *vfs_text_string = "Glyphs obtained from VFS";
		{
			Timer::Connection timer(_env);

			Genode::uint64_t const start_us = timer.elapsed_us();

			enum { ITERATIONS = 40 };
			for (int i = 0; i < ITERATIONS; i++)
				Text_painter::paint(_surface,
				                    Text_painter::Position(260 + (i*133 % 500),
				                                           320 + (i*87  % 400)),
				                    _font_4, Color(150 + i*73, 0, 200),
				                    "Glyphs obtained from VFS");

			Genode::uint64_t const end_us = timer.elapsed_us();
			unsigned long num_glyphs = strlen(vfs_text_string)*ITERATIONS;

			log("uncached painting: ", (float)(end_us - start_us)/num_glyphs, " us/glyph");
			_refresh();
		}

		for (size_t limit_kib = 32; limit_kib < 192; limit_kib += 16)
		{
			Cached_font cached_font(_heap, _font_4, Cached_font::Limit{limit_kib*1024});

			Timer::Connection timer(_env);

			Genode::uint64_t const start_us = timer.elapsed_us();

			/* use less iterations for small cache sizes */
			int const iterations = (limit_kib < 100) ? 200 : 2000;
			for (int i = 0; i < iterations; i++)
				Text_painter::paint(_surface,
				                    Text_painter::Position(260 + (i*83  % 500),
				                                           320 + (i*153 % 400)),
				                    cached_font, Color(30, limit_kib, 150 + i*73),
				                    "Glyphs obtained from VFS");

			Genode::uint64_t const end_us = timer.elapsed_us();
			unsigned long num_glyphs = strlen(vfs_text_string)*iterations;

			log("cached painting:   ", (float)(end_us - start_us)/num_glyphs, " us/glyph"
			    " (", cached_font.stats(), ")");
			_refresh();
		}
	}
};


void Component::construct(Genode::Env &env)
{
	static Test::Main main(env);
}


/*
 * Resolve symbol required by libc. It is unused as we implement
 * 'Component::construct' directly instead of initializing the libc.
 */

#include <libc/component.h>

void Libc::Component::construct(Libc::Env &) { }

