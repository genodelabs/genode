/*
 * \brief  Nitpicker pointer with support for VirtualBox-defined shapes
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/pixel_rgb565.h>
#include <nitpicker_session/connection.h>

/* local includes */
#include "util.h"
#include "policy.h"
#include "big_mouse.h"


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


class Main : public Vbox_pointer::Pointer_updater
{
	private:

		typedef Vbox_pointer::String          String;
		typedef Vbox_pointer::Policy          Policy;
		typedef Vbox_pointer::Policy_registry Policy_registry;

		Genode::Attached_rom_dataspace _hover_ds { "hover" };
		Genode::Attached_rom_dataspace _xray_ds  { "xray" };

		Genode::Signal_receiver _sig_rec;

		Genode::Signal_dispatcher<Main> _hover_signal_dispatcher {
			_sig_rec, *this, &Main::_handle_hover };
		Genode::Signal_dispatcher<Main> _xray_signal_dispatcher {
			_sig_rec, *this, &Main::_handle_xray };

		Nitpicker::Connection _nitpicker;

		Nitpicker::Session::View_handle _view = _nitpicker.create_view();

		Policy_registry _policy_registry { *this };

		String _hovered_label;
		String _hovered_domain;

		bool _xray                    = false;
		bool _default_pointer_visible = false;

		Nitpicker::Area              _current_pointer_size;
		Genode::Dataspace_capability _pointer_ds;

		void _resize_nitpicker_buffer_if_needed(Nitpicker::Area pointer_size);
		void _show_default_pointer();
		void _show_shape_pointer(Policy *p);
		void _update_pointer();
		void _handle_hover(unsigned num = 0);
		void _handle_xray(unsigned num = 0);

	public:

		Main();

		/*******************************
		 ** Pointer_updater interface **
		 *******************************/

		void update_pointer(Policy *policy) override;

		Genode::Signal_receiver & signal_receiver() override { return _sig_rec; }
};


void Main::_resize_nitpicker_buffer_if_needed(Nitpicker::Area pointer_size)
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


void Main::_show_default_pointer()
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

	Genode::Attached_dataspace ds { _pointer_ds };

	convert_default_pointer_data_to_pixels(ds.local_addr<Genode::Pixel_rgb565>(),
	                                       pointer_size);
	_nitpicker.framebuffer()->refresh(0, 0, pointer_size.w(), pointer_size.h());

	Nitpicker::Rect geometry(Nitpicker::Point(0, 0), pointer_size);
	_nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(_view, geometry);
	_nitpicker.execute();

	_default_pointer_visible = true;
}


void Main::_show_shape_pointer(Policy *p)
{
	try {
		_resize_nitpicker_buffer_if_needed(p->shape_size());
	} catch (...) {
		error(__func__, ": could not resize the pointer buffer "
		      "for ", p->shape_size(), " pixels");
		throw;
	}

	Genode::Attached_dataspace ds { _pointer_ds };

	p->draw_shape(ds.local_addr<Genode::Pixel_rgb565>());

	_nitpicker.framebuffer()->refresh(0, 0, p->shape_size().w(), p->shape_size().h());

	Nitpicker::Rect geometry(p->shape_hot(), p->shape_size());
	_nitpicker.enqueue<Nitpicker::Session::Command::Geometry>(_view, geometry);
	_nitpicker.execute();

	_default_pointer_visible = false;
}


void Main::_update_pointer()
{
	Policy *policy = nullptr;

	if (_xray
	 || !(policy = _policy_registry.lookup(_hovered_label, _hovered_domain))
	 || !policy->shape_valid())
		_show_default_pointer();
	else
		try {
			_show_shape_pointer(policy);
		} catch (...) { _show_default_pointer(); }
}


void Main::_handle_hover(unsigned)
{
	using Vbox_pointer::read_string_attribute;

	_hover_ds.update();
	if (!_hover_ds.valid())
		return;

	/* read new hover information from nitpicker's hover report */
	try {
		Genode::Xml_node node(_hover_ds.local_addr<char>());

		String hovered_label  = read_string_attribute(node, "label",  String());
		String hovered_domain = read_string_attribute(node, "domain", String());

		/* update pointer if hovered domain or label changed */
		if (hovered_label != _hovered_label || hovered_domain != _hovered_domain) {
			_hovered_label  = hovered_label;
			_hovered_domain = hovered_domain;
			_update_pointer();
		}
	}
	catch (...) {
		Genode::warning("could not parse hover report");
	}
}


void Main::_handle_xray(unsigned)
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


void Main::update_pointer(Policy *policy)
{
	/* update pointer if shape-changing policy is hovered */
	if (policy == _policy_registry.lookup(_hovered_label, _hovered_domain))
		_update_pointer();
}


Main::Main()
{
	/*
	 * Try to allocate the Nitpicker buffer for the maximum supported
	 * pointer size to let the user know right from the start if the
	 * RAM quota is too low.
	 */
	Framebuffer::Mode const mode { Vbox_pointer::MAX_WIDTH, Vbox_pointer::MAX_HEIGHT,
	                               Framebuffer::Mode::RGB565 };

	_nitpicker.buffer(mode, true /* use alpha */);

	_policy_registry.update(Genode::config()->xml_node());

	/* register signal handlers */
	_hover_ds.sigh(_hover_signal_dispatcher);
	_xray_ds.sigh(_xray_signal_dispatcher);

	_nitpicker.enqueue<Nitpicker::Session::Command::To_front>(_view);
	_nitpicker.execute();

	/* import initial state */
	_handle_hover();
	_handle_xray();
	_update_pointer();
}


int main()
{
	static Main main;

	/* dispatch signals */
	for (;;) {

		Genode::Signal sig = main.signal_receiver().wait_for_signal();
		Genode::Signal_dispatcher_base *dispatcher =
			dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}
}
