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
#include <os/pixel_rgb888.h>
#include <os/pixel_rgb565.h>
#include <os/surface.h>
#include <os/texture_rgb888.h>
#include <gui_session/connection.h>
#include <report_rom/report_service.h>
#include <pointer/dither_painter.h>
#include <pointer/shape_report.h>

/* local includes */
#include "rom_registry.h"
#include "big_mouse.h"

namespace Pointer { class Main; }


template <typename PT>
void convert_default_pointer_data_to_pixels(PT *pixel, Gui::Area size)
{
	unsigned char *alpha = (unsigned char *)(pixel + size.count());

	for (unsigned y = 0; y < size.h; y++) {
		for (unsigned x = 0; x < size.w; x++) {

			/* the source is known to be in RGB888 format */
			Genode::Pixel_rgb565 src =
				*(Genode::Pixel_rgb565 *)(&big_mouse.pixels[y][x]);

			unsigned const i = y*size.w + x;
			pixel[i] = PT(src.r(), src.g(), src.b());
			alpha[i] = src.r() ? 255 : 0;
		}
	}
}


class Pointer::Main : public Rom::Reader
{
	private:

		using String = Genode::String<128>;

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		bool _verbose = _config.xml().attribute_value("verbose", false);

		Gui::Connection _gui { _env };

		Gui::View_id _view = _gui.create_view();

		bool _default_pointer_visible = false;

		Gui::Area _current_pointer_size { };

		Genode::Dataspace_capability _pointer_ds { };

		void _resize_gui_buffer_if_needed(Gui::Area pointer_size);
		void _show_default_pointer();
		void _update_pointer();

		/* custom shape support */

		bool _shapes_enabled = _config.xml().attribute_value("shapes", false);

		bool _xray = false;

		Genode::Constructible<Genode::Attached_rom_dataspace> _hover_ds { };
		Genode::Constructible<Genode::Attached_rom_dataspace> _xray_ds  { };

		Genode::Signal_handler<Main> _hover_signal_handler {
			_env.ep(), *this, &Main::_handle_hover };
		Genode::Signal_handler<Main> _xray_signal_handler {
			_env.ep(), *this, &Main::_handle_xray };

		Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

		Rom::Registry _rom_registry { _sliced_heap, _env.ram(), _env.rm(), *this };

		Report::Root _report_root { _env, _sliced_heap, _rom_registry, _verbose };

		Genode::Session_label _hovered_label { };

		Genode::Attached_ram_dataspace _texture_pixel_ds { _env.ram(), _env.rm(),
		                                                   Pointer::MAX_WIDTH  *
		                                                   Pointer::MAX_HEIGHT *
		                                                   sizeof(Genode::Pixel_rgb888) };

		Genode::Attached_ram_dataspace _texture_alpha_ds { _env.ram(), _env.rm(),
		                                                   Pointer::MAX_WIDTH  *
		                                                   Pointer::MAX_HEIGHT };

		void _show_shape_pointer(Shape_report &shape_report);
		void _handle_hover();
		void _handle_xray();

		/**
		 * Reader interface
		 */
		void mark_as_outdated()    override { }
		void mark_as_invalidated() override { }
		void notify_client()       override { _update_pointer(); }

	public:

		Main(Genode::Env &);
};


void Pointer::Main::_resize_gui_buffer_if_needed(Gui::Area pointer_size)
{
	if (pointer_size == _current_pointer_size)
		return;

	Framebuffer::Mode const mode { .area = pointer_size };

	_gui.buffer(mode, true /* use alpha */);

	_pointer_ds = _gui.framebuffer.dataspace();

	_current_pointer_size = pointer_size;
}


void Pointer::Main::_show_default_pointer()
{
	/* only draw default pointer if not already drawn */
	if (_default_pointer_visible)
		return;

	Gui::Area const pointer_size { big_mouse.w, big_mouse.h };

	try {
		_resize_gui_buffer_if_needed(pointer_size);
	} catch (...) {
		Genode::error(__func__, ": could not resize the pointer buffer "
		              "for ", pointer_size.w, "x", pointer_size.h, " pixels");
		return;
	}

	Genode::Attached_dataspace ds { _env.rm(), _pointer_ds };

	convert_default_pointer_data_to_pixels(ds.local_addr<Genode::Pixel_rgb888>(),
	                                       pointer_size);
	_gui.framebuffer.refresh(0, 0, pointer_size.w, pointer_size.h);

	Gui::Rect geometry(Gui::Point(0, 0), pointer_size);
	_gui.enqueue<Gui::Session::Command::Geometry>(_view, geometry);
	_gui.execute();

	_default_pointer_visible = true;
}


void Pointer::Main::_show_shape_pointer(Shape_report &shape_report)
{
	Gui::Area  shape_size;
	Gui::Point shape_hot;

	if (shape_report.visible) {

		shape_size = Gui::Area(shape_report.width, shape_report.height);
		shape_hot = Gui::Point((int)-shape_report.x_hot, (int)-shape_report.y_hot);

		try {
			_resize_gui_buffer_if_needed(shape_size);
		} catch (...) {
			error(__func__, ": could not resize the pointer buffer "
			      "for ", shape_size, " pixels");
			throw;
		}

		using namespace Genode;

		/* import shape into texture */

		Texture<Pixel_rgb888>
			texture(_texture_pixel_ds.local_addr<Pixel_rgb888>(),
			        _texture_alpha_ds.local_addr<unsigned char>(),
			        shape_size);

		for (unsigned int y = 0; y < shape_size.h; y++) {

			/* import the RGBA-encoded line into the texture */
			unsigned char *shape = shape_report.shape;
			unsigned char *line  = &shape[y * shape_size.w * 4];
			texture.rgba(line, shape_size.w, y);
		}

		/* draw texture */

		Attached_dataspace ds { _env.rm(), _pointer_ds };

		Pixel_rgb888 *pixel = ds.local_addr<Pixel_rgb888>();

		Pixel_alpha8 *alpha =
			reinterpret_cast<Pixel_alpha8 *>(pixel + shape_size.count());

		Surface<Pixel_rgb888> pixel_surface(pixel, shape_size);
		Surface<Pixel_alpha8> alpha_surface(alpha, shape_size);

		Dither_painter::paint(pixel_surface, texture);
		Dither_painter::paint(alpha_surface, texture);
	}

	_gui.framebuffer.refresh(0, 0, shape_size.w, shape_size.h);

	Gui::Rect geometry(shape_hot, shape_size);
	_gui.enqueue<Gui::Session::Command::Geometry>(_view, geometry);
	_gui.execute();

	_default_pointer_visible = false;
}


void Pointer::Main::_update_pointer()
{
	if (!_shapes_enabled || _xray) {
		_show_default_pointer();
		return;
	}

	try {

		Rom::Readable_module &shape_module =
			_rom_registry.lookup(*this, _hovered_label);

		try {
			Shape_report shape_report { 0, 0, 0, 0, 0, { 0 } };

			shape_module.read_content(*this, (char*)&shape_report, sizeof(shape_report));

			/* show default pointer on invisible/empty/invalid shape report */
			if (!shape_report.visible ||
			    ((shape_report.width == 0) ||
			     (shape_report.height == 0) ||
			     (shape_report.width > MAX_WIDTH) ||
			     (shape_report.height > MAX_HEIGHT)))
			    throw Genode::Exception();

			_show_shape_pointer(shape_report);

		} catch (...) {
			_rom_registry.release(*this, shape_module);
			throw;
		}

		_rom_registry.release(*this, shape_module);

	} catch (...) {
		_show_default_pointer();
	}
}


void Pointer::Main::_handle_hover()
{
	_hover_ds->update();
	if (!_hover_ds->valid())
		return;

	/* read new hover information from nitpicker's hover report */
	try {
		Genode::Xml_node node(_hover_ds->local_addr<char>());

		Genode::Session_label hovered_label {
			node.attribute_value("label", String()) };

		hovered_label = hovered_label.prefix();

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


void Pointer::Main::_handle_xray()
{
	_xray_ds->update();
	if (!_xray_ds->valid())
		return;

	try {
		Genode::Xml_node node(_xray_ds->local_addr<char>());

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

Pointer::Main::Main(Genode::Env &env) : _env(env)
{
	/*
	 * Try to allocate the Nitpicker buffer for the maximum supported
	 * pointer size to let the user know right from the start if the
	 * RAM quota is too low.
	 */
	Framebuffer::Mode const mode { .area = { Pointer::MAX_WIDTH, Pointer::MAX_HEIGHT } };

	_gui.buffer(mode, true /* use alpha */);

	if (_shapes_enabled) {
		try {
			_hover_ds.construct(_env, "hover");
			_hover_ds->sigh(_hover_signal_handler);
			_handle_hover();
		} catch (Genode::Rom_connection::Rom_connection_failed) {
			Genode::warning("Could not open ROM session for \"hover\".",
		                    " This ROM is used for custom pointer shape support.");
		}

		try {
			_xray_ds.construct(_env, "xray");
			_xray_ds->sigh(_xray_signal_handler);
			_handle_xray();
		} catch (Genode::Rom_connection::Rom_connection_failed) {
			Genode::warning("Could not open ROM session for \"xray\".",
		                    " This ROM is used for custom pointer shape support.");
		}
	}

	_gui.enqueue<Gui::Session::Command::Front>(_view);
	_gui.execute();

	_update_pointer();

	if (_shapes_enabled) {
		/* announce 'Report' service */
		env.parent().announce(env.ep().manage(_report_root));
	}
}


void Component::construct(Genode::Env &env) { static Pointer::Main main(env); }
