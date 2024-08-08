/*
 * \brief  Minimalistic status bar for nitpicker
 * \author Norman Feske
 * \date   2014-07-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/reconstructible.h>
#include <util/xml_node.h>
#include <util/color.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/pixel_rgb888.h>
#include <gui_session/connection.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/tff_font.h>

namespace Status_bar {

	using namespace Genode;

	using Domain_name = String<128>;
	using Label       = String<128>;
	using Area        = Surface_base::Area;
	using Point       = Surface_base::Point;
	using Rect        = Surface_base::Rect;

	struct Buffer;
	struct Main;
}

extern char _binary_default_tff_start;


struct Status_bar::Buffer
{
	enum { HEIGHT = 18, LABEL_GAP = 15 };

	Gui::Connection &_gui;

	Framebuffer::Mode const _nit_mode { _gui.mode() };

	/*
	 * Dimension nitpicker buffer depending on nitpicker's screen size.
	 * The status bar is as wide as nitpicker's screen and has a fixed
	 * height.
	 */
	Framebuffer::Mode const _mode { .area = { _nit_mode.area.w, HEIGHT } };

	Dataspace_capability _init_buffer()
	{
		_gui.buffer(_mode, false);
		return _gui.framebuffer.dataspace();
	}

	Attached_dataspace _fb_ds;

	Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };
	Tff_font _font { &_binary_default_tff_start, _glyph_buffer };

	Buffer(Region_map &rm, Gui::Connection &gui)
	:
		_gui(gui), _fb_ds(rm, _init_buffer())
	{ }

	template <typename PT>
	void _draw_outline(Surface<PT> &surface, Point pos, char const *s)
	{
		for (int j = -1; j <= 1; j++)
			for (int i = -1; i <= 1; i++)
				if (i || j)
					Text_painter::paint(surface,
					                    Text_painter::Position(pos.x + i, pos.y + j),
					                    _font, Color::black(), s);
	}

	template <typename PT>
	void _draw_label(Surface<PT> &surface, Point pos,
	                 Domain_name const &domain_name, Label const &label,
	                 Color color)
	{
		Color const label_text_color = Color::clamped_rgb((color.r + 255)/2,
		                                                  (color.g + 255)/2,
		                                                  (color.b + 255)/2);
		Color const domain_text_color = Color::rgb(255, 255, 255);

		pos = pos + Point(1, 1);

		_draw_outline(surface, pos, domain_name.string());
		Text_painter::paint(surface, Text_painter::Position(pos.x, pos.y),
		                    _font, domain_text_color, domain_name.string());

		pos = pos + Point(_font.string_width(domain_name.string()).decimal() + LABEL_GAP, 0);

		_draw_outline(surface, pos, label.string());
		Text_painter::paint(surface, Text_painter::Position(pos.x, pos.y),
		                    _font, label_text_color, label.string());
	}

	Area _label_size(Domain_name const &domain_name, Label const &label) const
	{
		return Area(_font.string_width(domain_name.string()).decimal() + LABEL_GAP
		          + _font.string_width(label.string()).decimal() + 2,
		            _font.bounding_box().h + 2);
	}

	void draw(Domain_name const &, Label const &, Color);

	Framebuffer::Mode mode() const { return _mode; }
};


void Status_bar::Buffer::draw(Domain_name const &domain_name,
                              Label       const &label,
                              Color              color)
{
	using PT = Pixel_rgb888;

	Area const area = _mode.area;

	Surface<PT> surface(_fb_ds.local_addr<PT>(), area);

	Rect const view_rect(Point(0, 0), area);

	unsigned r = color.r, g = color.g, b = color.b;

	/* dim session color a bit to improve the contrast of the label */
	r = (r + 100)/2, g = (g + 100)/2, b = (b + 100)/2;

	/* highlight first line with slightly brighter color */
	Box_painter::paint(surface,
	                   Rect(Point(0, 0), Area(view_rect.w(), 1)),
	                   Color::clamped_rgb(r + (r / 2), g + (g / 2), b + (b / 2)));

	/* draw slightly shaded background */
	for (unsigned i = 1; i < area.h - 1; i++) {
		r -= r > 3 ? 4 : 0;
		g -= g > 3 ? 4 : 0;
		b -= b > 4 ? 4 : 0;

		Box_painter::paint(surface,
		                   Rect(Point(0, i), Area(view_rect.w(), 1)),
		                   Color::clamped_rgb(r, g, b));
	}

	/* draw last line darker */
	Box_painter::paint(surface,
	                   Rect(Point(0, view_rect.h() - 1), Area(view_rect.w(), 1)),
	                   Color::clamped_rgb(r / 4, g / 4, b / 4));

	_draw_label(surface, view_rect.center(_label_size(domain_name, label)),
	            domain_name, label, color);

	_gui.framebuffer.refresh(0, 0, area.w, area.h);
}


struct Status_bar::Main
{
	Env &_env;

	Attached_rom_dataspace _focus_ds { _env, "focus" };

	void _handle_focus();

	Signal_handler<Main> _focus_handler {
		_env.ep(), *this, &Main::_handle_focus };

	void _handle_mode();

	Signal_handler<Main> _mode_handler {
		_env.ep(), *this, &Main::_handle_mode };

	Gui::Connection _gui { _env, "status_bar" };

	/* status-bar attributes */
	Domain_name _domain_name { };
	Label       _label       { };
	Color       _color       { };

	Reconstructible<Buffer> _buffer { _env.rm(), _gui };

	Gui::View_id const _view { _gui.create_view() };

	void _draw_status_bar()
	{
		_buffer->draw(_domain_name, _label, _color);
	}

	Main(Env &env) : _env(env)
	{
		/* register signal handlers */
		_focus_ds.sigh(_focus_handler);
		_gui.mode_sigh(_mode_handler);

		/* schedule initial view-stacking command, needed only once */
		_gui.enqueue<Gui::Session::Command::Front>(_view);

		/* import initial state */
		_handle_mode();
		_handle_focus();
	}
};


void Status_bar::Main::_handle_focus()
{
	/* fetch new content of the focus ROM module */
	_focus_ds.update();
	if (!_focus_ds.valid())
		return;

	/* reset status-bar properties */
	_label       = Label();
	_domain_name = Domain_name();
	_color       = Color::black();

	/* read new focus information from nitpicker's focus report */
	try {
		Xml_node node(_focus_ds.local_addr<char>());

		_label       = node.attribute_value("label",  Label());
		_domain_name = node.attribute_value("domain", Domain_name());
		_color       = node.attribute_value("color",  Color::black());
	}
	catch (...) {
		warning("could not parse focus report"); }

	_draw_status_bar();
}


void Status_bar::Main::_handle_mode()
{
	_buffer.construct(_env.rm(), _gui);

	_draw_status_bar();

	Rect const geometry(Point(0, 0), _buffer->mode().area);

	_gui.enqueue<Gui::Session::Command::Geometry>(_view, geometry);
	_gui.execute();
}


void Component::construct(Genode::Env &env) { static Status_bar::Main main { env }; }
