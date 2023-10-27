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
#include <base/sleep.h>
#include <input/event.h>
#include <os/reporter.h>
#include <timer_session/connection.h>
#include <os/vfs.h>

/* gems includes */
#include <gems/gui_buffer.h>

namespace Menu_view { struct Main; }


struct Menu_view::Main
{
	Env &_env;

	Gui::Connection _gui { _env };

	Constructible<Gui_buffer> _buffer { };

	Gui::Session::View_handle _view_handle = _gui.create_view();

	/**
	 * Dialog position in screen coordinate space
	 */
	Point _position { };

	/**
	 * Last pointer position at the time of the most recent hovering report,
	 * in screen coordinate space.
	 */
	Point _hovered_position { };

	bool _dialog_hovered = false;

	Area _configured_size { };
	Area _visible_size { };

	Area _root_widget_size() const
	{
		Area const min_size = _root_widget.min_size();
		return Area(max(_configured_size.w(), min_size.w()),
		            max(_configured_size.h(), min_size.h()));
	}

	Rect _view_geometry { };

	void _update_view(Rect geometry)
	{
		if (_view_geometry.p1()   == geometry.p1()
		 && _view_geometry.area() == geometry.area())
			return;

		/* display view behind all others */
		typedef Gui::Session::Command     Command;
		typedef Gui::Session::View_handle View_handle;

		_view_geometry = geometry;
		_gui.enqueue<Command::Geometry>(_view_handle, _view_geometry);
		_gui.enqueue<Command::To_front>(_view_handle, View_handle());
		_gui.execute();
	}

	/**
	 * Function called on config change or mode change
	 */
	void _handle_dialog_update();

	Signal_handler<Main> _dialog_update_handler = {
		_env.ep(), *this, &Main::_handle_dialog_update};

	Attached_rom_dataspace _config { _env, "config" };

	Heap _heap { _env.ram(), _env.rm() };

	Vfs::Env &_vfs_env;

	Directory _root_dir   { _vfs_env };
	Directory _fonts_dir  { _root_dir, "fonts" };
	Directory _styles_dir { _root_dir, "styles" };

	Style_database _styles { _env.ram(), _env.rm(), _heap, _fonts_dir, _styles_dir,
	                         _dialog_update_handler };

	Animator _animator { };

	Widget_factory _widget_factory { _heap, _styles, _animator };

	Root_widget _root_widget { _widget_factory, Xml_node("<dialog/>"), Widget::Unique_id() };

	Attached_rom_dataspace _dialog_rom { _env, "dialog" };

	Attached_dataspace _input_ds { _env.rm(), _gui.input()->dataspace() };

	bool _opaque = false;

	Color _background_color { };

	Widget::Hovered _last_reported_hovered { };

	void _handle_config();

	Signal_handler<Main> _config_handler = {
		_env.ep(), *this, &Main::_handle_config};

	struct Input_seq_number
	{
		Constructible<Input::Seq_number> _curr { };

		bool _processed = false;

		void update(Input::Seq_number const &seq_number)
		{
			_curr.construct(seq_number);
			_processed = false;
		}

		bool changed() const { return _curr.constructed() && !_processed; }

		void mark_as_processed() { _processed = true; }

		void generate(Xml_generator &xml)
		{
			if (_curr.constructed())
				xml.attribute("seq_number", _curr->value);
		}

	} _input_seq_number { };

	void _handle_input();

	Signal_handler<Main> _input_handler = {
		_env.ep(), *this, &Main::_handle_input};

	/*
	 * Timer used for animating widgets
	 */
	struct Frame_timer : Timer::Connection
	{
		enum { PERIOD = 10 };

		Genode::uint64_t curr_frame() const { return elapsed_ms() / PERIOD; }

		void schedule() { trigger_once((Genode::uint64_t)Frame_timer::PERIOD*1000); }

		Frame_timer(Env &env) : Timer::Connection(env) { }

	} _timer { _env };

	void _handle_frame_timer();

	Signal_handler<Main> _frame_timer_handler = {
		_env.ep(), *this, &Main::_handle_frame_timer};

	Constructible<Genode::Expanding_reporter> _hover_reporter { };

	void _update_hover_report();

	bool _schedule_redraw = false;

	/**
	 * Frame of last call of 'handle_frame_timer'
	 */
	Genode::uint64_t _last_frame = 0;

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

	Main(Env &env, Vfs::Env &libc_vfs_env)
	:
		_env(env), _vfs_env(libc_vfs_env)
	{
		_dialog_rom.sigh(_dialog_update_handler);
		_config.sigh(_config_handler);

		_gui.input()->sigh(_input_handler);

		_timer.sigh(_frame_timer_handler);

		/* apply initial configuration */
		_handle_config();
	}
};


void Menu_view::Main::_update_hover_report()
{
	if (!_hover_reporter.constructed())
		return;

	if (!_dialog_hovered) {
		_hover_reporter->generate([&] (Xml_generator &) { });
		return;
	}

	Widget::Hovered const new_hovered = _root_widget.hovered(_hovered_position);

	bool const hover_changed = (_last_reported_hovered != new_hovered);

	if (hover_changed || _input_seq_number.changed()) {

		_hover_reporter->generate([&] (Xml_generator &xml) {
			_input_seq_number.generate(xml);
			_root_widget.gen_hover_model(xml, _hovered_position);
		});

		_last_reported_hovered = new_hovered;
		_input_seq_number.mark_as_processed();
	}
}


void Menu_view::Main::_handle_dialog_update()
{
	_styles.flush_outdated_styles();

	Xml_node const config = _config.xml();

	_position        = Point::from_xml(config);
	_configured_size = Area ::from_xml(config);

	_dialog_rom.update();

	Xml_node dialog = _dialog_rom.xml();

	if (dialog.has_type("empty"))
		return;

	_root_widget.update(dialog);
	_root_widget.size(_root_widget_size());

	_update_hover_report();

	_schedule_redraw = true;

	/*
	 * If we have not processed a period for at least one frame, perform the
	 * processing immediately. This way, we avoid latencies when the dialog
	 * model is updated sporadically.
	 */
	Genode::uint64_t const curr_frame = _timer.curr_frame();
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

	Xml_node const config = _config.xml();

	config.with_optional_sub_node("report", [&] (Xml_node const &report) {
		_hover_reporter.conditional(report.attribute_value("hover", false),
		                            _env, "hover", "hover");
	});

	_opaque = config.attribute_value("opaque", false);

	_background_color = config.attribute_value("background", Color(127, 127, 127, 255));

	config.with_optional_sub_node("vfs", [&] (Xml_node const &vfs_node) {
		_vfs_env.root_dir().apply_config(vfs_node); });

	_handle_dialog_update();
}


void Menu_view::Main::_handle_input()
{
	Point const orig_hovered_position = _hovered_position;
	bool  const orig_dialog_hovered   = _dialog_hovered;

	_gui.input()->for_each_event([&] (Input::Event const &ev) {

		ev.handle_seq_number([&] (Input::Seq_number seq_number) {
			_input_seq_number.update(seq_number); });

		auto hover_at = [&] (int x, int y)
		{
			_dialog_hovered   = true;
			_hovered_position = Point(x, y) - _position;
		};

		auto unhover = [&] ()
		{
			_dialog_hovered   = false;
			_hovered_position = Point();
		};

		ev.handle_absolute_motion([&] (int x, int y) {
			hover_at(x, y); });

		ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
			if (id.value == 0)
				hover_at((int)x, (int)y); });

		/*
		 * Reset hover model when losing the focus
		 */
		if (ev.hover_leave())
			unhover();
	});

	bool const hover_changed = orig_dialog_hovered   != _dialog_hovered
	                        || orig_hovered_position != _hovered_position;

	if (hover_changed || _input_seq_number.changed())
		_update_hover_report();
}


void Menu_view::Main::_handle_frame_timer()
{
	_frame_cnt++;

	Genode::uint64_t const curr_frame = _timer.curr_frame();

	if (_animator.active()) {

		Genode::uint64_t const passed_frames = max(curr_frame - _last_frame, 4U);

		if (passed_frames > 0) {

			for (Genode::uint64_t i = 0; i < passed_frames; i++)
				_animator.animate();

			_schedule_redraw = true;
		}
	}

	_last_frame = curr_frame;

	if (_schedule_redraw && _frame_cnt >= REDRAW_PERIOD) {

		_frame_cnt = 0;

		Area const size = _root_widget_size();

		unsigned const buffer_w = _buffer.constructed() ? _buffer->size().w() : 0,
		               buffer_h = _buffer.constructed() ? _buffer->size().h() : 0;

		Area const max_size(max(buffer_w, size.w()), max(buffer_h, size.h()));

		bool const size_increased = (max_size.w() > buffer_w)
		                         || (max_size.h() > buffer_h);

		if (!_buffer.constructed() || size_increased)
			_buffer.construct(_gui, max_size, _env.ram(), _env.rm(),
			                  _opaque ? Gui_buffer::Alpha::OPAQUE
			                          : Gui_buffer::Alpha::ALPHA,
			                  _background_color);
		else
			_buffer->reset_surface();

		_root_widget.position(Point(0, 0));

		// XXX restrict redraw to dirty regions
		//     don't perform a full dialog update
		_buffer->apply_to_surface([&] (Surface<Pixel_rgb888> &pixel,
		                               Surface<Pixel_alpha8> &alpha) {
			_root_widget.draw(pixel, alpha, Point(0, 0));
		});

		_buffer->flush_surface();
		_gui.framebuffer()->refresh(0, 0, _buffer->size().w(), _buffer->size().h());
		_update_view(Rect(_position, size));

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


Menu_view::Widget &
Menu_view::Widget_factory::create(Xml_node const &node)
{
	Widget::Unique_id const unique_id(++_unique_id_cnt);

	if (node.has_type("label"))    return *new (alloc) Label_widget      (*this, node, unique_id);
	if (node.has_type("button"))   return *new (alloc) Button_widget     (*this, node, unique_id);
	if (node.has_type("vbox"))     return *new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("hbox"))     return *new (alloc) Box_layout_widget (*this, node, unique_id);
	if (node.has_type("frame"))    return *new (alloc) Frame_widget      (*this, node, unique_id);
	if (node.has_type("float"))    return *new (alloc) Float_widget      (*this, node, unique_id);
	if (node.has_type("depgraph")) return *new (alloc) Depgraph_widget   (*this, node, unique_id);

	/*
	 * This cannot occur because the 'List_model' ensures that 'create' is only
	 * called for nodes that passed 'node_type_known'.
	 */
	error("unknown widget type '", node.type(), "'");
	sleep_forever();
}


bool Menu_view::Widget_factory::node_type_known(Xml_node const &node)
{
	return node.has_type("label")
	    || node.has_type("button")
	    || node.has_type("vbox")
	    || node.has_type("hbox")
	    || node.has_type("frame")
	    || node.has_type("float")
	    || node.has_type("depgraph");
}


/*
 * Silence debug messages
 */
extern "C" void _sigprocmask() { }


void Libc::Component::construct(Libc::Env &env)
{
	static Menu_view::Main main(env, env.vfs_env());
}

