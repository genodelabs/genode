/*
 * \brief  Menu for Live CD
 * \author Norman Feske
 * \date   2010-09-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>
#include <base/thread.h>
#include <rom_session/connection.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <audio_out_session/connection.h>
#include <base/allocator_avl.h>

/* local includes */
#include "banner.h"
#include "child.h"

#include <rom_file.h>

enum { IMAGE_NAME_MAX_LEN = 64 };


class Menu_entry : public Genode::List<Menu_entry>::Element
{
	private:

		Nano3d::Rect _sensor;
		Nano3d::Rect _view;
		bool         _hover;
		bool         _selected;
		bool         _needs_update;
		int          _blend;

	public:

		Menu_entry(Nano3d::Rect sensor, Nano3d::Rect view)
		:
			_sensor(sensor), _view(view),
			_hover(false), _selected(false), _needs_update(true),
			_blend(0)
		{ }

		bool is_located_at(Nano3d::Point p)
		{
			return (p.x() >= _sensor.x1()
			     && p.y() >= _sensor.y1()
			     && p.x() <= _sensor.x2()
			     && p.y() <= _sensor.y2());
		}

		void hover(bool hover)
		{
			if (hover == _hover)
				return;

			/* start blending if hover gets released */
			if (_hover && !hover) {
				_blend = 256;
			} else {
				_blend = 0;
			}

			_hover = hover;
			_needs_update = true;
		}

		virtual void click()
		{
			_selected = !_selected;
			_needs_update = true;
		}

		bool hover() { return _hover; }
		bool selected() { return _selected; }
		bool needs_update() { return _needs_update; }
		int blend() { return _blend; }

		void update_done()
		{
			_needs_update = false;

			/* keep updating if blending is not yet finished */
			if (_blend > 0) {
				enum { BLEND_SPEED = 24 };
				_blend = Genode::max(0, _blend - BLEND_SPEED);
				_needs_update = true;
			}
		}

		Nano3d::Rect view() { return _view; }
};


class Menu
{
	public:

		enum Image {
			IMG_DEFAULT, IMG_HOVER, IMG_SELECTED, IMG_HSELECTED,
			IMG_MAX
		};

	private:

		/**
		 * Shortcut for pixel type
		 */
		typedef Genode::uint16_t PT;

		/**
		 * Shortcut for alpha type
		 */
		typedef unsigned char AT;

		int                          _xpos, _ypos;
		Png_image                    _png_image;
		Lazy_value<int>              _fade_in_pos;
		Nitpicker::Connection        _nitpicker;
		Framebuffer::Mode            _mode;
		Framebuffer::Session_client  _framebuffer;
		Genode::Dataspace_capability _fb_ds;
		Nitpicker::View_capability   _view_cap;
		Nitpicker::View_client       _view;
		PT                          *_img_pixel[4];
		AT                          *_img_alpha[4];
		PT                          *_pixel;
		AT                          *_alpha;
		unsigned char               *_input_mask;
		Genode::List<Menu_entry>     _entries;

		Genode::Dataspace_capability _ev_ds;
		Input::Event                *_ev_buf;
		int                          _rmx, _rmy;
		int                          _key_cnt;
		Menu_entry                  *_focused_entry;

		int                          _next_banner_id;
		int                          _curr_banner_id;

		unsigned _width()      const { return _png_image.width(); }
		unsigned _height()     const { return _png_image.height(); }
		unsigned _num_pixels() const { return _width()*_height(); }

		Framebuffer::Session_capability _init_framebuffer()
		{
			_nitpicker.buffer(_mode, true);
			return _nitpicker.framebuffer_session();
		}

		void _draw_entry(Menu_entry *entry)
		{
			if (!entry) return;
			Nano3d::Rect view = entry->view();

			/* select source image */
			Image source = IMG_DEFAULT;
			if (entry->selected()) {
				if (_img_pixel[IMG_SELECTED])
					source = IMG_SELECTED;
				if (entry->hover() && _img_pixel[IMG_HSELECTED])
					source = IMG_HSELECTED;
			} else {
				if (entry->hover() && _img_pixel[IMG_HOVER])
					source = IMG_HOVER;
			}

			/* determine blending source */
			Image blend_source = IMG_DEFAULT;
			if (source == IMG_DEFAULT && _img_pixel[IMG_HOVER])
				blend_source = IMG_HOVER;
			if (source == IMG_SELECTED && _img_pixel[IMG_HSELECTED])
				blend_source = IMG_HSELECTED;

			unsigned start_offset = view.y1()*_width() + view.x1();
			PT *src_pixel = _img_pixel[source] + start_offset;
			AT *src_alpha = _img_alpha[source] + start_offset;
			PT *blend_pixel = _img_pixel[blend_source] + start_offset;
			AT *blend_alpha = _img_alpha[blend_source] + start_offset;
			PT *dst_pixel = _pixel + start_offset;
			AT *dst_alpha = _alpha + start_offset;
			unsigned long line_len = _width();

			int blend_value = entry->blend();
			int anti_blend_value = 256 - blend_value;

			if (blend_value) {

				for (unsigned y = 0; y < view.h(); y++) {

					/* blend line */
					for (unsigned x = 0; x < view.w(); x++) {
						PT s = src_pixel[x];
						enum { RED_MASK   = 0xf800,
						       GREEN_MASK = 0x07e0,
						       BLUE_MASK  = 0x001f };
						int sr = (s & RED_MASK);
						int sg = (s & GREEN_MASK);
						int sb = (s & BLUE_MASK);
						PT b = blend_pixel[x];
						int br = (b & RED_MASK);
						int bg = (b & GREEN_MASK);
						int bb = (b & BLUE_MASK);

						int dr = sr*anti_blend_value + br*blend_value;
						int dg = sg*anti_blend_value + bg*blend_value;
						int db = sb*anti_blend_value + bb*blend_value;

						PT d = ((dr >> 8) & RED_MASK)
						     | ((dg >> 8) & GREEN_MASK)
						     | ((db >> 8) & BLUE_MASK);

						dst_pixel[x] = d;
						dst_alpha[x] = src_alpha[x];
					}

					src_pixel += line_len;
					src_alpha += line_len;
					blend_pixel += line_len;
					blend_alpha += line_len;
					dst_pixel += line_len;
					dst_alpha += line_len;
				}

			} else {
				for (unsigned y = 0; y < view.h(); y++) {

					/* copy line */
					for (unsigned x = 0; x < view.w(); x++) {
						dst_pixel[x] = src_pixel[x];
						dst_alpha[x] = src_alpha[x];
					}

					src_pixel += line_len;
					src_alpha += line_len;
					dst_pixel += line_len;
					dst_alpha += line_len;
				}
			}
		}

	public:

		void assign_image(Image img, void *png_image_data)
		{
			using namespace Genode;

			if (_img_pixel[img]) env()->heap()->free(_img_pixel[img], _num_pixels());
			if (_img_alpha[img]) env()->heap()->free(_img_alpha[img], _num_pixels());

			_img_pixel[img] = (PT *)env()->heap()->alloc(_num_pixels()*sizeof(PT));
			_img_alpha[img] = (AT *)env()->heap()->alloc(_num_pixels());

			Png_image png(png_image_data);
			png.convert_to_rgb565(_img_pixel[img], _img_alpha[img],
			                      _width(), _height());
		}

		Menu(void *png_image_data, int xpos, int ypos)
		:
			_xpos(xpos), _ypos(ypos),
			_png_image(png_image_data),
			_fade_in_pos(-_png_image.height() << 8),
			_nitpicker(false),
			_mode(_png_image.width(), _png_image.height(), Framebuffer::Mode::RGB565),
			_framebuffer(_init_framebuffer()),
			_fb_ds(_framebuffer.dataspace()),
			_view_cap(_nitpicker.create_view()),
			_view(_view_cap),
			_pixel(Genode::env()->rm_session()->attach(_fb_ds)),
			_alpha((AT *)_pixel + _num_pixels()*sizeof(PT)),
			_input_mask(_alpha + _num_pixels()),
			_ev_ds(_nitpicker.input()->dataspace()),
			_ev_buf(Genode::env()->rm_session()->attach(_ev_ds)),
			_rmx(0), _rmy(0), _key_cnt(0),
			_focused_entry(0),
			_next_banner_id(Banner::INITIAL),
			_curr_banner_id(Banner::INITIAL)
		{
			_fade_in_pos.dst(_ypos << 8, 16);

			for (unsigned i = 0; i < IMG_MAX; i++) {
				_img_pixel[i] = 0;
				_img_alpha[i] = 0;
			}

			assign_image(IMG_DEFAULT, png_image_data);


			/*
			 * Initialize Nitpicker buffer with default image data.
			 */
			for (unsigned i = 0; i < _num_pixels(); i++) {
				_pixel[i] = _img_pixel[IMG_DEFAULT][i];
				_alpha[i] = _img_alpha[IMG_DEFAULT][i];
			}

			/*
			 * Fix up color values for zero-alpha pixels as these
			 * colors are visible in Nitpicker's X-ray mode.
			 * First, find first non-zero-alpha pixel value.
			 * Then replace all zero-alpha pixels with that value.
			 */
			PT corner_color = 0;
			for (unsigned i = 0; i < _num_pixels(); i++) {
				if (_alpha[i]) {
					corner_color = _pixel[i];
					break;
				}
			}

			for (unsigned i = 0; i < _num_pixels(); i++)
				if (_alpha[i] == 0)
					_pixel[i] = corner_color;

			/*
			 * Initialize input mask such that drop shadows go through
			 * the drop shadow.
			 */
			enum { SHADOW_MAX_ALPHA = 120 };
			for (unsigned i = 0; i < _width()*_height(); i++)
				_input_mask[i] = (_alpha[i] > SHADOW_MAX_ALPHA);

			_view.stack(Nitpicker::View_capability(), true, true);
		}

		void add_entry(Menu_entry *entry) { _entries.insert(entry); }

		void handle_input()
		{
			if (!_nitpicker.input()->is_pending()) return;

			for (int i = 0, num_ev = _nitpicker.input()->flush(); i < num_ev; i++) {

				Input::Event *ev = &_ev_buf[i];

				if (ev->type() == Input::Event::PRESS)   _key_cnt++;
				if (ev->type() == Input::Event::RELEASE) _key_cnt--;

				int x = ev->ax() - _xpos;
				int y = ev->ay() - _ypos;

				if (_key_cnt == 0) {
					Menu_entry *entry = _entries.first();
					for (; entry; entry = entry->next()) {
						if (entry->is_located_at(Nano3d::Point(x,y))) {
							if (_focused_entry != entry) {
								_focused_entry = entry;
							}
							entry->hover(true);
							break;
						} else {
							entry->hover(false);
						}
					}
					if (!entry && _focused_entry) {
						_focused_entry = 0;
					}
				}

				if (_focused_entry
				 && ev->type() == Input::Event::PRESS
				 && ev->code() == Input::BTN_LEFT) {
					_focused_entry->click();
				}
			}
		}

		void update()
		{
			bool any_entry_updated = false;
			Menu_entry *entry = _entries.first();
			for (; entry; entry = entry->next())
				if (entry->needs_update()) {
					_draw_entry(entry);
					any_entry_updated = true;
					entry->update_done();
				}

			if (any_entry_updated)
				_nitpicker.framebuffer()->refresh(0, 0, _width(), _height());

			if ((_fade_in_pos >> 8) != (_fade_in_pos.dst() >> 8)) {
				_ypos = _fade_in_pos >> 8;
				_fade_in_pos.animate();
				_view.viewport(_xpos, _ypos, _width(), _height(), 0, 0, true);
			}
		}
};


static void assign_image_to_menu(Genode::Xml_node menu_xml,
                                 const char *image_tag_name,
                                 Menu *menu, Menu::Image img)
{
	try {
		char buf[IMAGE_NAME_MAX_LEN];
		menu_xml.sub_node(image_tag_name).attribute("png").value(buf, sizeof(buf));
		Rom_file *rom_file = new (Genode::env()->heap()) Rom_file(buf);
		menu->assign_image(img, rom_file->local_addr());
	} catch (...) { }
}


class Launcher_menu_entry : public Menu_entry
{
	private:

		Menu_child *_child;

		Genode::Xml_node          _xml_node;
		Genode::Service_registry *_parent_services;
		long                      _prio_levels_log2;
		Genode::Cap_session      *_cap_session;

	public:

		Launcher_menu_entry(Genode::Xml_node          xml_node,
		                    Genode::Service_registry *parent_services,
		                    long                      prio_levels_log2,
		                    Genode::Cap_session      *cap_session,
		                    Nano3d::Rect              sensor,
		                    Nano3d::Rect              view)
		:
			Menu_entry(sensor, view),
			_child(0),
			_xml_node(xml_node),
			_parent_services(parent_services),
			_prio_levels_log2(prio_levels_log2),
			_cap_session(cap_session)
		{ }

		/**************************
		 ** Menu_entry interface **
		 **************************/

		void click()
		{
			Menu_entry::click();
			if (selected() && _child == 0) {
				try {
					_child = new (Genode::env()->heap())
						Menu_child(_xml_node, _parent_services,
						           _prio_levels_log2, _cap_session);
				} catch (...) { }
			}
			if (!selected() && _child != 0) {
				Genode::destroy(Genode::env()->heap(), _child),
				_child = 0;
			}
		}
};


int main(int argc, char **argv)
{
	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	Genode::Cap_connection cap_session;
	long prio_levels_log2 = read_prio_levels_log2();
	Genode::Service_registry parent_services;

	Genode::Xml_node menu_xml = Genode::config()->xml_node().sub_node("menu");

	char image_name[IMAGE_NAME_MAX_LEN];
	menu_xml.sub_node("image").attribute("png").value(image_name, sizeof(image_name));

	static Rom_file menu_png_image(image_name);
	int menu_xpos = 16, menu_ypos = 16;
	static Menu menu(menu_png_image.local_addr(), menu_xpos, menu_ypos);

	assign_image_to_menu(menu_xml, "image-hover",     &menu, Menu::IMG_HOVER);
	assign_image_to_menu(menu_xml, "image-selected",  &menu, Menu::IMG_SELECTED);
	assign_image_to_menu(menu_xml, "image-hselected", &menu, Menu::IMG_HSELECTED);

	try {
		Genode::Xml_node entry = menu_xml.sub_node("entry");
		for (;;) {

			/* read sensor geometry */
			long sx = 0, sy = 0, sw = 0, sh = 0;
			entry.attribute("xpos").value(&sx);
			entry.attribute("ypos").value(&sy);
			entry.attribute("width").value(&sw);
			entry.attribute("height").value(&sh);

			/* set defaults for view geometry */
			long vx = sx, vy = sy, vw = sw, vh = sh;

			/* read customized view geometry if provided */
			try {
				Genode::Xml_node view = entry.sub_node("view");
				view.attribute("xpos").value(&vx);
				view.attribute("ypos").value(&vy);
				view.attribute("width").value(&vw);
				view.attribute("height").value(&vh);
			} catch (...) { }

			/* create and register menu entry */
			Menu_entry *menu_entry = new (Genode::env()->heap())
				Launcher_menu_entry(entry,
				                    &parent_services,
				                    prio_levels_log2,
				                    &cap_session,
				                    Nano3d::Rect(Nano3d::Point(sx, sy),
				                                 Nano3d::Area(sw, sh)),
				                    Nano3d::Rect(Nano3d::Point(vx, vy),
				                                 Nano3d::Area(vw, vh)));

			menu.add_entry(menu_entry);

			/* proceed with next XML node */
			entry = entry.next("entry");
		}
	} catch (...) { }

	Timer::Connection timer;
	while (1) {

		menu.handle_input();
		menu.update();
		timer.msleep(5);
	}
	return 0;
}
