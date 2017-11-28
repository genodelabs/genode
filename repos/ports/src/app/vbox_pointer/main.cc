/*
 * \brief  Nitpicker pointer with support for VirtualBox-defined shapes
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <os/pixel_alpha8.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_rgb888.h>
#include <os/surface.h>
#include <os/texture_rgb888.h>
#include <nitpicker_session/connection.h>
#include <report_rom/report_service.h>
#include <vbox_pointer/dither_painter.h>
#include <vbox_pointer/shape_report.h>

/* local includes */
#include "util.h"
#include "rom_registry.h"
#include "big_mouse.h"

namespace Vbox_pointer { class Main; }


template <typename PT>
void convert_default_pointer_data_to_pixels(PT *pixel, Nitpicker::Area size)
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


class Vbox_pointer::Main : public Rom::Reader
{
	private:

		typedef Vbox_pointer::String String;

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _hover_ds { _env, "hover"  };
		Genode::Attached_rom_dataspace _xray_ds  { _env, "xray"   };
		Genode::Attached_rom_dataspace _config   { _env, "config" };

		Genode::Signal_handler<Main> _hover_signal_handler {
			_env.ep(), *this, &Main::_handle_hover };
		Genode::Signal_handler<Main> _xray_signal_handler {
			_env.ep(), *this, &Main::_handle_xray };

		Nitpicker::Connection _nitpicker { _env };

		Nitpicker::Session::View_handle _view = _nitpicker.create_view();

		Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		Rom::Registry _rom_registry { _sliced_heap, _env.ram(), _env.rm(), *this };

		bool _verbose = _config.xml().attribute_value("verbose", false);

		Report::Root _report_root { _env, _sliced_heap, _rom_registry, _verbose };

		String _hovered_label;

		bool _xray                    = false;
		bool _default_pointer_visible = false;

		Nitpicker::Area              _current_pointer_size;
		Genode::Dataspace_capability _pointer_ds;

		Genode::Attached_ram_dataspace _texture_pixel_ds { _env.ram(), _env.rm(),
		                                                   Vbox_pointer::MAX_WIDTH  *
		                                                   Vbox_pointer::MAX_HEIGHT *
		                                                   sizeof(Genode::Pixel_rgb888) };

		Genode::Attached_ram_dataspace _texture_alpha_ds { _env.ram(), _env.rm(),
		                                                   Vbox_pointer::MAX_WIDTH  *
		                                                   Vbox_pointer::MAX_HEIGHT };

		void _resize_nitpicker_buffer_if_needed(Nitpicker::Area pointer_size);
		void _show_default_pointer();
		void _show_shape_pointer(Shape_report &shape_report);
		void _update_pointer();
		void _handle_hover();
		void _handle_xray();

	public:

		/**
		 * Reader interface
		 */
		void notify_module_changed() override
		{
			_update_pointer();
		}

		void notify_module_invalidated() override
		{
			_update_pointer();
		}

		Main(Genode::Env &);
};


void Vbox_pointer::Main::_resize_nitpicker_buffer_if_needed(Nitpicker::Area pointer_size)
{
	if (pointer_size == _current_pointer_size)
		return;

	Framebuffer::Mode const mode { (int)pointer_size.w(),
	                               (int)pointer_size.h(),
	                               Framebuffer::Mode::RGB565 };

	_nitpicker.buffer(mode, true /* use alpha */);

	_pointer_ds = _nitpicker.framebuffer()->dataspace();

	_current_pointer_size = pointer_size;
}


void Vbox_pointer::Main::_show_default_pointer()
{
	/* only draw default pointer if not already drawn */
	if (_default_pointer_visible)
		return;

	Nitpicker::Area const pointer_size { big_mouse.w, big_mouse.h };

	try {
		_resize_nitpicker_buffer_if_needed(pointer_size);
	} catch (...) {
		Genode::error(__func__, ": could not resize the pointer buffer "
		              "for ", pointer_size.w(), "x", pointer_size.h(), " pixels");
		return;
	}

	Genode::Attached_dataspace ds { _env.rm(), _pointer_ds };

	convert_default_pointer_data_to_pixels(ds.local_addr<Genode::Pixel_rgb565>(),
	                                       pointer_size);
	_nitpicker.framebuffer()->refresh(0, 0, pointer_size.w(), pointer_size.h());

	Nitpicker::Rect geometry(Nitpicker::Point(0, 0), pointer_size);
	_nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(_view, geometry);
	_nitpicker.execute();

	_default_pointer_visible = true;
}


void Vbox_pointer::Main::_show_shape_pointer(Shape_report &shape_report)
{
	Nitpicker::Area shape_size { shape_report.width, shape_report.height };
	Nitpicker::Point shape_hot { (int)-shape_report.x_hot, (int)-shape_report.y_hot };

	try {
		_resize_nitpicker_buffer_if_needed(shape_size);
	} catch (...) {
		error(__func__, ": could not resize the pointer buffer "
		      "for ", shape_size, " pixels");
		throw;
	}

	if (shape_report.visible) {

		using namespace Genode;

		/* import shape into texture */

		Texture<Pixel_rgb888>
			texture(_texture_pixel_ds.local_addr<Pixel_rgb888>(),
			        _texture_alpha_ds.local_addr<unsigned char>(),
			        shape_size);

		for (unsigned int y = 0; y < shape_size.h(); y++) {

			/* import the RGBA-encoded line into the texture */
			unsigned char *shape = shape_report.shape;
			unsigned char *line  = &shape[y * shape_size.w() * 4];
			texture.rgba(line, shape_size.w(), y);
		}

		/* draw texture */

		Attached_dataspace ds { _env.rm(), _pointer_ds };

		Pixel_rgb565 *pixel = ds.local_addr<Pixel_rgb565>();

		Pixel_alpha8 *alpha =
			reinterpret_cast<Pixel_alpha8 *>(pixel + shape_size.count());

		Surface<Pixel_rgb565> pixel_surface(pixel, shape_size);
		Surface<Pixel_alpha8> alpha_surface(alpha, shape_size);

		Dither_painter::paint(pixel_surface, texture);
		Dither_painter::paint(alpha_surface, texture);
	}

	_nitpicker.framebuffer()->refresh(0, 0, shape_size.w(), shape_size.h());

	Nitpicker::Rect geometry(shape_hot, shape_size);
	_nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(_view, geometry);
	_nitpicker.execute();

	_default_pointer_visible = false;
}


void Vbox_pointer::Main::_update_pointer()
{
	if (_xray) {
		_show_default_pointer();
		return;
	}

	try {

		Rom::Readable_module &shape_module =
			_rom_registry.lookup(*this, _hovered_label);

		try {
			Shape_report shape_report;

			shape_module.read_content(*this, (char*)&shape_report, sizeof(shape_report));

			if (shape_report.visible) {

				if ((shape_report.width == 0) ||
				    (shape_report.height == 0) ||
				    (shape_report.width > MAX_WIDTH) ||
				    (shape_report.height > MAX_HEIGHT))
				    throw Genode::Exception();

				_show_shape_pointer(shape_report);
			}

		} catch (...) {
			_rom_registry.release(*this, shape_module);
			throw;
		}

		_rom_registry.release(*this, shape_module);

	} catch (...) {
		_show_default_pointer();
	}
}


void Vbox_pointer::Main::_handle_hover()
{
	using Vbox_pointer::read_string_attribute;

	_hover_ds.update();
	if (!_hover_ds.valid())
		return;

	/* read new hover information from nitpicker's hover report */
	try {
		Genode::Xml_node node(_hover_ds.local_addr<char>());

		String hovered_label  = read_string_attribute(node, "label",  String());

		if (_verbose)
			Genode::log("hovered_label: ", hovered_label);

		/* update pointer if hovered label changed */
		if (hovered_label != _hovered_label) {
			_hovered_label  = hovered_label;
			_update_pointer();
		}
	}
	catch (...) {
		Genode::warning("could not parse hover report");
	}
}


void Vbox_pointer::Main::_handle_xray()
{
	_xray_ds.update();
	if (!_xray_ds.valid())
		return;

	try {
		Genode::Xml_node node(_xray_ds.local_addr<char>());

		bool xray = node.attribute_value("enabled", false);

		/* update pointer if xray status changed */
		if (xray != _xray) {
			_xray = xray;
			_update_pointer();
		}
	}
	catch (...) {
		Genode::warning("could not parse xray report");
	}
}

Vbox_pointer::Main::Main(Genode::Env &env) : _env(env)
{
	/*
	 * Try to allocate the Nitpicker buffer for the maximum supported
	 * pointer size to let the user know right from the start if the
	 * RAM quota is too low.
	 */
	Framebuffer::Mode const mode { Vbox_pointer::MAX_WIDTH, Vbox_pointer::MAX_HEIGHT,
	                               Framebuffer::Mode::RGB565 };

	_nitpicker.buffer(mode, true /* use alpha */);

	/* register signal handlers */
	_hover_ds.sigh(_hover_signal_handler);
	_xray_ds.sigh(_xray_signal_handler);

	_nitpicker.enqueue<Nitpicker::Session::Command::To_front>(_view);
	_nitpicker.execute();

	/* import initial state */
	_handle_hover();
	_handle_xray();
	_update_pointer();

	/* announce 'Report' service */
	env.parent().announce(env.ep().manage(_report_root));
}


void Component::construct(Genode::Env &env) { static Vbox_pointer::Main main(env); }
