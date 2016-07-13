/*
 * \brief  Minimalistic status bar for nitpicker
 * \author Norman Feske
 * \date   2014-07-08
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/volatile_object.h>
#include <util/xml_node.h>
#include <util/color.h>
#include <os/attached_rom_dataspace.h>
#include <os/pixel_rgb565.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/text_painter.h>

typedef Genode::Color               Color;
typedef Genode::String<128>         Domain_name;
typedef Genode::String<128>         Label;
typedef Genode::Surface_base::Area  Area;
typedef Genode::Surface_base::Point Point;
typedef Genode::Surface_base::Rect  Rect;
typedef Genode::size_t              size_t;


/***************
 ** Utilities **
 ***************/

template <size_t N>
static Genode::String<N> read_string_attribute(Genode::Xml_node node, char const *attr,
                                               Genode::String<N> default_value)
{
	try {
		char buf[N];
		node.attribute(attr).value(buf, sizeof(buf));
		return Genode::String<N>(Genode::Cstring(buf));
	}
	catch (...) {
		return default_value; }
}

extern char _binary_default_tff_start;


/******************
 ** Main program **
 ******************/

struct Status_bar_buffer
{
	enum { HEIGHT = 18, LABEL_GAP = 15 };

	Nitpicker::Connection &nitpicker;

	Framebuffer::Mode const nit_mode { nitpicker.mode() };

	/*
	 * Dimension nitpicker buffer depending on nitpicker's screen size.
	 * The status bar is as wide as nitpicker's screen and has a fixed
	 * height.
	 */
	Framebuffer::Mode const mode { nit_mode.width(), HEIGHT, nit_mode.format() };

	Genode::Dataspace_capability init_buffer()
	{
		nitpicker.buffer(mode, false);
		return nitpicker.framebuffer()->dataspace();
	}

	Genode::Attached_dataspace fb_ds { init_buffer() };

	Text_painter::Font const font { &_binary_default_tff_start };

	Status_bar_buffer(Nitpicker::Connection &nitpicker)
	:
		nitpicker(nitpicker)
	{ }

	template <typename PT>
	void _draw_outline(Genode::Surface<PT> &surface, Point pos, char const *s)
	{
		for (int j = -1; j <= 1; j++)
			for (int i = -1; i <= 1; i++)
				if (i || j)
					Text_painter::paint(surface, pos + Point(i, j), font,
					                    Color(0, 0, 0), s);
	}

	template <typename PT>
	void _draw_label(Genode::Surface<PT> &surface, Point pos,
	                 Domain_name const &domain_name, Label const &label,
	                 Color color)
	{
		Color const label_text_color((color.r + 255)/2,
		                              (color.g + 255)/2,
		                              (color.b + 255)/2);
		Color const domain_text_color(255, 255, 255);

		pos = pos + Point(1, 1);

		_draw_outline(surface, pos, domain_name.string());
		Text_painter::paint(surface, pos, font, domain_text_color,
		                    domain_name.string());

		pos = pos + Point(font.str_w(domain_name.string()) + LABEL_GAP, 0);

		_draw_outline(surface, pos, label.string());
		Text_painter::paint(surface, pos, font, label_text_color, label.string());
	}

	Area _label_size(Domain_name const &domain_name, Label const &label) const
	{
		return Area(font.str_w(domain_name.string()) + LABEL_GAP
		          + font.str_w(label.string()) + 2,
		            font.str_h(domain_name.string()) + 2);
	}

	void draw(Domain_name const &, Label const &, Color);
};


void Status_bar_buffer::draw(Domain_name const &domain_name,
                             Label       const &label,
                             Color              color)
{
	if (mode.format() != Framebuffer::Mode::RGB565) {
		Genode::error("pixel format not supported");
		return;
	}

	typedef Genode::Pixel_rgb565 PT;

	Area const area(mode.width(), mode.height());

	Genode::Surface<PT> surface(fb_ds.local_addr<PT>(), area);

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

	nitpicker.framebuffer()->refresh(0, 0, area.w(), area.h());
}


struct Main
{
	Genode::Attached_rom_dataspace focus_ds { "focus" };

	Genode::Signal_receiver sig_rec;

	void handle_focus(unsigned);

	Genode::Signal_dispatcher<Main> focus_signal_dispatcher {
		sig_rec, *this, &Main::handle_focus };

	void handle_mode(unsigned);

	Genode::Signal_dispatcher<Main> mode_signal_dispatcher {
		sig_rec, *this, &Main::handle_mode };

	Nitpicker::Connection nitpicker;

	/* status-bar attributes */
	Domain_name domain_name;
	Label       label;
	Color       color;

	Genode::Volatile_object<Status_bar_buffer> status_bar_buffer { nitpicker };

	Nitpicker::Session::View_handle const view { nitpicker.create_view() };

	void draw_status_bar()
	{
		status_bar_buffer->draw(domain_name, label, color);
	}

	Main()
	{
		/* register signal handlers */
		focus_ds.sigh(focus_signal_dispatcher);
		nitpicker.mode_sigh(mode_signal_dispatcher);

		/* schedule initial view-stacking command, needed only once */
		nitpicker.enqueue<Nitpicker::Session::Command::To_front>(view);

		/* import initial state */
		handle_mode(0);
		handle_focus(0);
	}
};


void Main::handle_focus(unsigned)
{
	/* fetch new content of the focus ROM module */
	focus_ds.update();
	if (!focus_ds.valid())
		return;

	/* reset status-bar properties */
	label       = Label();
	domain_name = Domain_name();
	color       = Color(0, 0, 0);

	/* read new focus information from nitpicker's focus report */
	try {
		Genode::Xml_node node(focus_ds.local_addr<char>());

		label       = read_string_attribute(node, "label", Label());
		domain_name = read_string_attribute(node, "domain", Domain_name());
		color       = node.attribute_value("color", Color(0, 0, 0));
	}
	catch (...) {
		Genode::warning("could not parse focus report"); }

	draw_status_bar();
}


void Main::handle_mode(unsigned)
{
	status_bar_buffer.construct(nitpicker);

	draw_status_bar();

	Rect const geometry(Point(0, 0), Area(status_bar_buffer->mode.width(),
	                                      status_bar_buffer->mode.height()));

	nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(view, geometry);
	nitpicker.execute();
}


int main(int, char **)
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

	return 0;
}
