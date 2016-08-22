/*
 * \brief  Pointer shape reporter test
 * \author Christian Helmuth
 * \date   2015-06-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <util/string.h>
#include <os/reporter.h>
#include <os/config.h>
#include <vbox_pointer/shape_report.h>


static bool const verbose = false;


typedef Genode::String<16> String;

static String read_string_attribute(Genode::Xml_node const &node,
                                    char             const *attr,
                                    String           const &default_value)
{
	try {
		char buf[String::capacity()];
		node.attribute(attr).value(buf, sizeof(buf));
		return String(Genode::Cstring(buf));
	}
	catch (...) {
		return default_value; }
}


struct Shape
{
	enum { WIDTH = 16, HEIGHT = 16 };

	String        const id;
	unsigned      const x_hot;
	unsigned      const y_hot;
	unsigned char const map[WIDTH*HEIGHT];
};


struct Shape_report : Vbox_pointer::Shape_report
{
	Genode::Reporter reporter { "shape", "shape", sizeof(Vbox_pointer::Shape_report) };

	Shape_report()
	:
		Vbox_pointer::Shape_report{true, 0, 0, Shape::WIDTH, Shape::HEIGHT, { 0 }}
	{
		reporter.enabled(true);
	}

	void report(Shape const &s)
	{
		x_hot = s.x_hot;
		y_hot = s.y_hot;

		unsigned const w = Shape::WIDTH;
		unsigned const h = Shape::HEIGHT;

		for (unsigned y = 0; y < h; ++y) {
			for (unsigned x = 0; x < w; ++x) {
				shape[(y*w + x)*4 + 0] = 0xff;
				shape[(y*w + x)*4 + 1] = 0xff;
				shape[(y*w + x)*4 + 2] = 0xff;
				shape[(y*w + x)*4 + 3] = s.map[y*w +x] ? 0xe0 : 0;

				if (verbose)
					Genode::printf("%c", s.map[y*w +x] ? 'X' : ' ');
			}
			if (verbose)
				Genode::printf("\n");
		}

		if (verbose)
			Genode::printf(".%s.%u.%u.\n", s.id.string(), s.x_hot, s.y_hot);

		reporter.report(static_cast<Vbox_pointer::Shape_report *>(this),
		                sizeof(Vbox_pointer::Shape_report));
	}
};

static Shape const shape[] = {
	{ "arrow", 0, 0, {
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
		0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
		0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
		0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,
		0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,
		0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,
		0,0,0,0,0,0,0,0,1,0,1,1,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,0,
		0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
	{ "blade", 0, 0, {
		1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,1,0,1,0,1,1,0,0,
		0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,
		0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,
		0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,
		0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
	{ "bladex", 8, 8, {
		1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,
		1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1,
		0,1,0,1,0,0,0,0,0,0,0,0,1,0,1,0,
		0,0,1,0,1,0,0,0,0,0,0,1,0,1,0,0,
		0,0,0,1,0,1,0,0,0,0,1,0,1,0,0,0,
		0,0,0,0,1,0,1,0,0,1,0,1,0,0,0,0,
		0,0,0,0,0,1,0,1,1,0,1,0,0,0,0,0,
		0,0,0,0,0,0,1,0,1,1,0,0,0,0,0,0,
		0,0,0,0,0,0,1,1,0,1,0,0,0,0,0,0,
		0,0,1,1,0,1,0,1,1,0,1,0,1,1,0,0,
		0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0,
		0,0,0,1,1,1,0,0,0,0,1,1,1,0,0,0,
		0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0,
		0,1,1,1,0,1,1,0,0,1,1,0,1,1,1,0,
		0,1,1,0,0,0,0,0,0,0,0,0,0,1,1,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } },
	{ "smiley", 8, 8, {
		0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
		0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,
		0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,
		0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,
		0,1,0,0,0,1,1,0,0,1,1,0,0,0,1,0,
		1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1,
		1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
		0,1,0,0,1,1,0,0,0,0,1,1,0,0,1,0,
		0,1,0,0,0,0,1,1,1,1,0,0,0,0,1,0,
		0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,
		0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,
		0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0 } },
	{ "yelims", 8, 8, {
		0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
		0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,
		0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,
		0,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,
		0,1,0,0,0,1,1,0,0,1,1,0,0,0,1,0,
		1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1,
		1,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
		1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,1,
		0,1,0,0,1,1,0,0,0,0,1,1,0,0,1,0,
		0,1,0,1,0,0,0,0,0,0,0,0,1,0,1,0,
		0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,
		0,0,0,1,1,0,0,0,0,0,0,1,1,0,0,0,
		0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0 } }
};


static Shape const & select_shape()
{
	String const id = read_string_attribute(Genode::config()->xml_node(),
	                                        "shape", String("arrow"));

	for (Shape const &s : shape)
		if (s.id == id)
			return s;

	/* not found -> use first as default */
	return shape[0];
}


int main()
{
	static Shape_report r;

	/* register signal handler for config changes */
	Genode::Signal_receiver sig_rec;
	Genode::Signal_context  sig_ctx;

	Genode::config()->sigh(sig_rec.manage(&sig_ctx));

	while (true) {
		r.report(select_shape());
		sig_rec.wait_for_signal();
		Genode::config()->reload();
	}
}
