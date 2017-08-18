/*
 * \brief  Menu view
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "widget_factory.h"
#include "button_widget.h"
#include "label_widget.h"
#include "box_layout_widget.h"
#include "root_widget.h"
#include "float_widget.h"
#include "frame_widget.h"
#include "depgraph_widget.h"

/* Genode includes */
#include <input/event.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* gems includes */
#include <gems/nitpicker_buffer.h>

namespace Menu_view { struct Main; }


struct Menu_view::Main
{
	Env &_env;

	Nitpicker::Connection _nitpicker { _env };

	Constructible<Nitpicker_buffer> _buffer;

	Nitpicker::Session::View_handle _view_handle = _nitpicker.create_view();

	Point _position;

	Rect _view_geometry;

	void _update_view()
	{
		if (_view_geometry.p1() == _position
		 && _view_geometry.area() == _buffer->size())
			return;

		/* display view behind all others */
		typedef Nitpicker::Session::Command Command;

		_view_geometry = Rect(_position, _buffer->size());
		_nitpicker.enqueue<Command::Geometry>(_view_handle, _view_geometry);
		_nitpicker.enqueue<Command::To_front>(_view_handle);
		_nitpicker.execute();
	}

	/**
	 * Function called on config change or mode change
	 */
	void _handle_dialog_update();

	Signal_handler<Main> _dialog_update_handler = {
		_env.ep(), *this, &Main::_handle_dialog_update};

	Heap _heap { _env.ram(), _env.rm() };

	Style_database _styles { _env.ram(), _env.rm(), _heap };

	Animator _animator;

	Widget_factory _widget_factory { _heap, _styles, _animator };

	Root_widget _root_widget { _widget_factory, Xml_node("<dialog/>"), Widget::Unique_id() };

	Attached_rom_dataspace _dialog_rom { _env, "dialog" };

	Attached_dataspace _input_ds { _env.rm(), _nitpicker.input()->dataspace() };

	Widget::Unique_id _hovered;

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	void _handle_input();

	Signal_handler<Main> _input_handler = {
		_env.ep(), *this, &Main::_handle_input};

	/*
	 * Timer used for animating widgets
	 */
	struct Frame_timer : Timer::Connection
	{
		enum { PERIOD = 10 };

		unsigned curr_frame() const { return elapsed_ms() / PERIOD; }

		void schedule() { trigger_once(Frame_timer::PERIOD*1000); }

		Frame_timer(Env &env) : Timer::Connection(env) { }

	} _timer { _env };

	void _handle_frame_timer();

	Signal_handler<Main> _frame_timer_handler = {
		_env.ep(), *this, &Main::_handle_frame_timer};

	Genode::Reporter _hover_reporter = { _env, "hover" };

	bool _schedule_redraw = false;

	/**
	 * Frame of last call of 'handle_frame_timer'
	 */
	unsigned _last_frame = 0;

	/**
	 * Number of frames between two redraws
	 */
	enum { REDRAW_PERIOD = 2 };

	/**
	 * Counter used for triggering redraws. Incremented in each frame-timer
	 * period, wraps at 'REDRAW_PERIOD'. The redraw is performed when the
	 * counter wraps.
	 */
	unsigned _frame_cnt = 0;

	Main(Env &env) : _env(env)
	{
		_dialog_rom.sigh(_dialog_update_handler);
		_config.sigh(_config_handler);

		_nitpicker.input()->sigh(_input_handler);

		_timer.sigh(_frame_timer_handler);

		/* apply initial configuration */
		_handle_config();
	}
};


void Menu_view::Main::_handle_dialog_update()
{
	try {
		_position = Decorator::point_attribute(_config.xml());
	} catch (...) { }

	_dialog_rom.update();

	try {
		Xml_node dialog_xml(_dialog_rom.local_addr<char>());

		_root_widget.update(dialog_xml);
		_root_widget.size(_root_widget.min_size());
	} catch (...) {
		Genode::error("failed to construct widget tree");
	}

	_schedule_redraw = true;

	/*
	 * If we have not processed a period for at least one frame, perform the
	 * processing immediately. This way, we avoid latencies when the dialog
	 * model is updated sporadically.
	 */
	unsigned const curr_frame = _timer.curr_frame();
	if (curr_frame != _last_frame) {

		if (curr_frame - _last_frame > 10)
			_last_frame = curr_frame;

		_handle_frame_timer();
	} else {
		_timer.schedule();
	}
}


void Menu_view::Main::_handle_config()
{
	_config.update();

	try {
		_hover_reporter.enabled(_config.xml().sub_node("report")
		                                     .attribute_value("hover", false));
	} catch (...) {
		_hover_reporter.enabled(false);
	}

	_handle_dialog_update();
}


void Menu_view::Main::_handle_input()
{
	_nitpicker.input()->for_each_event([&] (Input::Event const &ev) {
		if (ev.absolute_motion()) {

			Point const at = Point(ev.ax(), ev.ay()) - _position;
			Widget::Unique_id const new_hovered = _root_widget.hovered(at);

			if (_hovered != new_hovered) {

				if (_hover_reporter.enabled()) {
					Genode::Reporter::Xml_generator xml(_hover_reporter, [&] () {
						_root_widget.gen_hover_model(xml, at);
					});
				}

				_hovered = new_hovered;
			}
		}

		/*
		 * Reset hover model when losing the focus
		 */
		if ((ev.type() == Input::Event::FOCUS && ev.code() == 0)
		 || (ev.type() == Input::Event::LEAVE)) {

			_hovered = Widget::Unique_id();

			if (_hover_reporter.enabled()) {
				Genode::Reporter::Xml_generator xml(_hover_reporter, [&] () { });
			}
		}
	});
}


void Menu_view::Main::_handle_frame_timer()
{
	_frame_cnt++;

	unsigned const curr_frame = _timer.curr_frame();

	if (_animator.active()) {

		unsigned const passed_frames = max(curr_frame - _last_frame, 4U);

		if (passed_frames > 0) {

			for (unsigned i = 0; i < passed_frames; i++)
				_animator.animate();

			_schedule_redraw = true;
		}
	}

	_last_frame = curr_frame;

	if (_schedule_redraw && _frame_cnt >= REDRAW_PERIOD) {

		_frame_cnt = 0;

		Area const old_size = _buffer.constructed() ? _buffer->size() : Area();
		Area const size     = _root_widget.min_size();

		if (!_buffer.constructed() || size.w() > old_size.w() || size.h() > old_size.h())
			_buffer.construct(_nitpicker, size, _env.ram(), _env.rm());
		else
			_buffer->reset_surface();

		_root_widget.size(size);
		_root_widget.position(Point(0, 0));

		Surface<Pixel_rgb888> pixel_surface = _buffer->pixel_surface();
		Surface<Pixel_alpha8> alpha_surface = _buffer->alpha_surface();

		// XXX restrict redraw to dirty regions
		//     don't perform a full dialog update
		_root_widget.draw(pixel_surface, alpha_surface, Point(0, 0));

		_buffer->flush_surface();
		_nitpicker.framebuffer()->refresh(0, 0, _buffer->size().w(), _buffer->size().h());
		_update_view();

		_schedule_redraw = false;
	}

	/*
	 * Deactivate timer periods when idle, activate timer when an animation is
	 * in progress or a redraw is pending.
	 */
	bool const redraw_pending = _schedule_redraw && _frame_cnt != 0;

	if (_animator.active() || redraw_pending)
		_timer.schedule();
}


Menu_view::Widget *
Menu_view::Widget_factory::create(Xml_node node)
{
	Widget *w = nullptr;

	Widget::Unique_id const unique_id(++_unique_id_cnt);

	if (node.has_type("label"))    w = new (alloc) Label_widget      (*this, node, unique_id);
	if (node.has_type("button"))   w = new (alloc) Button_widget     (*this, node, unique_id);
	if (node.has_type("vbox"))     w = new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("hbox"))     w = new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("frame"))    w = new (alloc) Frame_widget      (*this, node, unique_id);
	if (node.has_type("float"))    w = new (alloc) Float_widget      (*this, node, unique_id);
	if (node.has_type("depgraph")) w = new (alloc) Depgraph_widget   (*this, node, unique_id);

	if (!w) {
		Genode::error("unknown widget type '", node.type(), "'");
		return 0;
	}

	return w;
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }


void Libc::Component::construct(Libc::Env &env) { static Menu_view::Main main(env); }

