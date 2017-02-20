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
#include <os/pixel_rgb565.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/text_painter.h>

namespace Status_bar {

	using namespace Genode;

	typedef String<128>         Domain_name;
	typedef String<128>         Label;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Point Point;
	typedef Surface_base::Rect  Rect;

	struct Buffer;
	struct Main;
}

extern char _binary_default_tff_start;


struct Status_bar::Buffer
{
	enum { HEIGHT = 18, LABEL_GAP = 15 };

	Nitpicker::Connection &_nitpicker;

	Framebuffer::Mode const _nit_mode { _nitpicker.mode() };

	/*
	 * Dimension nitpicker buffer depending on nitpicker's screen size.
	 * The status bar is as wide as nitpicker's screen and has a fixed
	 * height.
	 */
	Framebuffer::Mode const _mode { _nit_mode.width(), HEIGHT, _nit_mode.format() };

	Dataspace_capability _init_buffer()
	{
		_nitpicker.buffer(_mode, false);
		return _nitpicker.framebuffer()->dataspace();
	}

	Attached_dataspace _fb_ds;

	Text_painter::Font const _font { &_binary_default_tff_start };

	Buffer(Region_map &rm, Nitpicker::Connection &nitpicker)
	:
		_nitpicker(nitpicker), _fb_ds(rm, _init_buffer())
	{ }

	template <typename PT>
	void _draw_outline(Surface<PT> &surface, Point pos, char const *s)
	{
		for (int j = -1; j <= 1; j++)
			for (int i = -1; i <= 1; i++)
				if (i || j)
					Text_painter::paint(surface, pos + Point(i, j), _font,
					                    Color(0, 0, 0), s);
	}

	template <typename PT>
	void _draw_label(Surface<PT> &surface, Point pos,
	                 Domain_name const &domain_name, Label const &label,
	                 Color color)
	{
		Color const label_text_color((color.r + 255)/2,
		                              (color.g + 255)/2,
		                              (color.b + 255)/2);
		Color const domain_text_color(255, 255, 255);

		pos = pos + Point(1, 1);

		_draw_outline(surface, pos, domain_name.string());
		Text_painter::paint(surface, pos, _font, domain_text_color,
		                    domain_name.string());

		pos = pos + Point(_font.str_w(domain_name.string()) + LABEL_GAP, 0);

		_draw_outline(surface, pos, label.string());
		Text_painter::paint(surface, pos, _font, label_text_color, label.string());
	}

	Area _label_size(Domain_name const &domain_name, Label const &label) const
	{
		return Area(_font.str_w(domain_name.string()) + LABEL_GAP
		          + _font.str_w(label.string()) + 2,
		            _font.str_h(domain_name.string()) + 2);
	}

	void draw(Domain_name const &, Label const &, Color);

	Framebuffer::Mode mode() const { return _mode; }
};


void Status_bar::Buffer::draw(Domain_name const &domain_name,
                              Label       const &label,
                              Color              color)
{
	if (_mode.format() != Framebuffer::Mode::RGB565) {
		error("pixel format not supported");
		return;
	}

	typedef Pixel_rgb565 PT;

	Area const area(_mode.width(), _mode.height());

	Surface<PT> surface(_fb_ds.local_addr<PT>(), area);

	Rect const view_rect(Point(0, 0), area);

	int r = color.r, g = color.g, b = color.b;

	/* dim session color a bit to improve the contrast of the label */
	r = (r + 100)/2, g = (g + 100)/2, b = (b + 100)/2;

	/* highlight first line with slightly brighter color */
	Box_painter::paint(surface,
	                   Rect(Point(0, 0), Area(view_rect.w(), 1)),
	                   Color(r + (r / 2), g + (g / 2), b + (b / 2)));

	/* draw slightly shaded background */
	for (unsigned i = 1; i < area.h() - 1; i++) {
		r -= r > 3 ? 4 : 0;
		g -= g > 3 ? 4 : 0;
		b -= b > 4 ? 4 : 0;

		Box_painter::paint(surface,
		                   Rect(Point(0, i), Area(view_rect.w(), 1)),
		                   Color(r, g, b));
	}

	/* draw last line darker */
	Box_painter::paint(surface,
	                   Rect(Point(0, view_rect.h() - 1), Area(view_rect.w(), 1)),
	                    Color(r / 4, g / 4, b / 4));

	_draw_label(surface, view_rect.center(_label_size(domain_name, label)),
	            domain_name, label, color);

	_nitpicker.framebuffer()->refresh(0, 0, area.w(), area.h());
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

	Nitpicker::Connection _nitpicker { _env, "status_bar" };

	/* status-bar attributes */
	Domain_name _domain_name;
	Label       _label;
	Color       _color;

	Reconstructible<Buffer> _buffer { _env.rm(), _nitpicker };

	Nitpicker::Session::View_handle const _view { _nitpicker.create_view() };

	void _draw_status_bar()
	{
		_buffer->draw(_domain_name, _label, _color);
	}

	Main(Env &env) : _env(env)
	{
		/* register signal handlers */
		_focus_ds.sigh(_focus_handler);
		_nitpicker.mode_sigh(_mode_handler);

		/* schedule initial view-stacking command, needed only once */
		_nitpicker.enqueue<Nitpicker::Session::Command::To_front>(_view);

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
	_color       = Color(0, 0, 0);

	/* read new focus information from nitpicker's focus report */
	try {
		Xml_node node(_focus_ds.local_addr<char>());

		_label       = node.attribute_value("label",  Label());
		_domain_name = node.attribute_value("domain", Domain_name());
		_color       = node.attribute_value("color",  Color(0, 0, 0));
	}
	catch (...) {
		warning("could not parse focus report"); }

	_draw_status_bar();
}


void Status_bar::Main::_handle_mode()
{
	_buffer.construct(_env.rm(), _nitpicker);

	_draw_status_bar();

	Rect const geometry(Point(0, 0), Area(_buffer->mode().width(),
	                                      _buffer->mode().height()));

	_nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(_view, geometry);
	_nitpicker.execute();
}


void Component::construct(Genode::Env &env) { static Status_bar::Main main { env }; }
