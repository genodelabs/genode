/*
 * \brief  Pointer shape reporter test
 * \author Christian Helmuth
 * \date   2015-06-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <util/string.h>
#include <os/reporter.h>
#include <pointer/shape_report.h>

struct Shape
{
	enum { WIDTH = 16, HEIGHT = 16 };

	typedef Genode::String<16> Id;

	Id            const id;
	bool          const visible;
	unsigned      const x_hot;
	unsigned      const y_hot;
	unsigned      const width;
	unsigned      const height;
	unsigned char const map[WIDTH*HEIGHT];

	void print(Genode::Output &output) const
	{
		Genode::print(output, ".", id, ".", x_hot, ".", y_hot, ".");
	}
};


static Shape const shape[] = {
	{ "arrow", true, 0, 0, Shape::WIDTH, Shape::HEIGHT, {
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
	{ "blade", true, 0, 0, 11, 11, {
		1,1,0,0,0,0,0,0,0,0,0,
		1,0,1,0,0,0,0,0,0,0,0,
		0,1,0,1,0,0,0,0,0,0,0,
		0,0,1,0,1,0,0,0,0,0,0,
		0,0,0,1,0,1,0,0,0,0,0,
		0,0,0,0,1,0,1,0,1,1,0,
		0,0,0,0,0,1,1,1,1,1,0,
		0,0,0,0,0,0,1,1,1,0,0,
		0,0,0,0,0,1,1,1,1,1,0,
		0,0,0,0,0,1,1,0,1,1,1,
		0,0,0,0,0,0,0,0,0,1,1 } },
	{ "bladex", true, 8, 8, Shape::WIDTH, Shape::HEIGHT, {
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
	{ "smiley", true, 8, 8, Shape::WIDTH, Shape::HEIGHT, {
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
	{ "yelims", true, 8, 8, Shape::WIDTH, Shape::HEIGHT, {
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
		0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0 } },
	{ "empty", true, 0, 0, 0, 0, { } },
	{ "invisible", false, 0, 0, Shape::WIDTH, Shape::HEIGHT, { } },
};


static Shape const &select_shape(Genode::Xml_node config)
{
	Shape::Id const id = config.attribute_value("shape", Shape::Id("arrow"));

	for (Shape const &s : shape)
		if (s.id == id)
			return s;

	/* not found -> use first as default */
	return shape[0];
}


struct Main
{
	Genode::Env &_env;

	Pointer::Shape_report _shape_report {
		true, 0, 0, Shape::WIDTH, Shape::HEIGHT, { 0 } };

	Genode::Reporter _reporter {
		_env, "shape", "shape", sizeof(Pointer::Shape_report) };

	Genode::Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	void _handle_config()
	{
		_config.update();

		Shape const &shape = select_shape(_config.xml());

		_shape_report.visible = shape.visible;
		_shape_report.x_hot   = shape.x_hot;
		_shape_report.y_hot   = shape.y_hot;
		_shape_report.width   = shape.width;
		_shape_report.height  = shape.height;

		unsigned const w = shape.width;
		unsigned const h = shape.height;

		for (unsigned y = 0; y < h; ++y) {
			for (unsigned x = 0; x < w; ++x) {
				_shape_report.shape[(y*w + x)*4 + 0] = 0xff;
				_shape_report.shape[(y*w + x)*4 + 1] = 0xf2;
				_shape_report.shape[(y*w + x)*4 + 2] = 0xac;
				_shape_report.shape[(y*w + x)*4 + 3] = shape.map[y*w + x] ? 0xe0 : 0;
			}
		}

		_reporter.report(&_shape_report, sizeof(Pointer::Shape_report));
	}

	Main(Genode::Env &env) : _env(env)
	{
		_reporter.enabled(true);
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main main(env);
}
