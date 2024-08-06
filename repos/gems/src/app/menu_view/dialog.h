/*
 * \brief  Top-level dialog
 * \author Norman Feske
 * \date   2024-04-03
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIALOG_H_
#define _DIALOG_H_

/* Genode include */
#include <input/event.h>

/* gems includes */
#include <gems/gui_buffer.h>

/* local includes */
#include <types.h>
#include <widget_factory.h>
#include <root_widget.h>

namespace Menu_view {

	struct Dialog;

	using Dialogs = List_model<Dialog>;
}


struct Menu_view::Dialog : List_model<Dialog>::Element
{
	Env &_env;

	Widget_factory &_global_widget_factory;

	Animator _local_animator { };

	Widget_factory _widget_factory { _global_widget_factory.alloc,
	                                 _global_widget_factory.styles,
	                                 _local_animator };

	struct Action : Interface
	{
		virtual void trigger_redraw() = 0;
		virtual void hover_changed() = 0;
		virtual void observed_seq_number(Input::Seq_number) = 0;
	};

	Action &_action;

	using Name = Widget::Name;

	Name const _name;

	static Name _name_from_attr(Xml_node const &node)
	{
		return node.attribute_value("name", Name());
	}

	Gui::Connection _gui { _env, _name };

	Attached_dataspace _input_ds { _env.rm(), _gui.input.dataspace() };

	Signal_handler<Dialog> _input_handler = {
		_env.ep(), *this, &Dialog::_handle_input};

	void _handle_input();

	Constructible<Gui_buffer> _buffer { };

	Gui::Session::View_handle const _view_handle = _gui.create_view();

	Point _position { };

	/**
	 * Last pointer position at the time of the most recent hovering report,
	 * in screen coordinate space.
	 */
	Point _hovered_position { };

	bool _hovered = false;
	bool _redraw_scheduled = false;

	Area _configured_size { };
	Area _visible_size    { };
	Rect _view_geometry   { };

	bool  _opaque = false;
	Color _background_color { };

	Area _root_widget_size() const
	{
		Area const min_size = _root_widget.min_size();
		return Area(max(_configured_size.w, min_size.w),
		            max(_configured_size.h, min_size.h));
	}

	void _update_view(Rect geometry)
	{
		if (_view_geometry.p1() == geometry.p1()
		 && _view_geometry.area == geometry.area)
			return;

		using Command = Gui::Session::Command;

		_view_geometry = geometry;
		_gui.enqueue<Command::Geometry>(_view_handle, _view_geometry);
		_gui.enqueue<Command::Front>(_view_handle);
		_gui.execute();
	}

	Root_widget _root_widget { _name, Widget::Unique_id { }, _widget_factory, Xml_node("<dialog/>") };

	Attached_rom_dataspace _dialog_rom { _env, _name.string() };

	Signal_handler<Dialog> _dialog_handler {
		_env.ep(), *this, &Dialog::_handle_dialog };

	void _handle_dialog();

	Dialog(Env &env, Widget_factory &widget_factory, Action &action,
	       Xml_node const &node)
	:
		_env(env), _global_widget_factory(widget_factory), _action(action),
		_name(_name_from_attr(node))
	{
		_dialog_rom.sigh(_dialog_handler);
		_dialog_handler.local_submit();
		_gui.input.sigh(_input_handler);
	}

	Widget::Hovered hovered_widget() const
	{
		return _root_widget.hovered(_hovered_position);
	}

	void gen_hover(Xml_generator &xml) const
	{
		if (_hovered)
			_root_widget.gen_hover_model(xml, _hovered_position);
	}

	void redraw()
	{
		if (!_redraw_scheduled)
			return;

		Area const size = _root_widget_size();

		unsigned const buffer_w = _buffer.constructed() ? _buffer->size().w : 0,
		               buffer_h = _buffer.constructed() ? _buffer->size().h : 0;

		Area const max_size(max(buffer_w, size.w), max(buffer_h, size.h));

		bool const size_increased = (max_size.w > buffer_w)
		                         || (max_size.h > buffer_h);

		if (!_buffer.constructed() || size_increased)
			_buffer.construct(_gui, max_size, _env.ram(), _env.rm(),
			                  _opaque ? Gui_buffer::Alpha::OPAQUE
			                          : Gui_buffer::Alpha::ALPHA,
			                  _background_color);
		else
			_buffer->reset_surface();

		_root_widget.position(Point(0, 0));

		_buffer->apply_to_surface([&] (Surface<Pixel_rgb888> &pixel,
		                               Surface<Pixel_alpha8> &alpha) {
			_root_widget.draw(pixel, alpha, Point(0, 0));
		});

		_buffer->flush_surface();
		_gui.framebuffer.refresh(0, 0, _buffer->size().w, _buffer->size().h);
		_update_view(Rect(_position, size));

		_redraw_scheduled = false;
	}

	bool hovered() const { return _hovered; }

	void animate()
	{
		bool const progress = _local_animator.active();

		_local_animator.animate();

		if (progress)
			_redraw_scheduled = true;
	}

	bool animation_in_progress() const { return _local_animator.active(); }

	bool redraw_scheduled() const { return _redraw_scheduled; }

	/*
	 * List_model
	 */

	static bool type_matches(Xml_node const &node) { return node.has_type("dialog"); }

	bool matches(Xml_node const &node) const { return _name_from_attr(node) == _name; }

	void update(Xml_node const &node)
	{
		_position         = Point::from_xml(node);
		_configured_size  = Area ::from_xml(node);
		_opaque           = node.attribute_value("opaque", false);
		_background_color = node.attribute_value("background", Color(127, 127, 127, 255));
	}
};


void Menu_view::Dialog::_handle_dialog()
{
	_dialog_rom.update();

	Xml_node const dialog = _dialog_rom.xml();

	if (dialog.has_type("empty"))
		return;

	_root_widget.update(dialog);
	_root_widget.size(_root_widget_size());

	_redraw_scheduled = true;

	_action.hover_changed();
	_action.trigger_redraw();
}


void Menu_view::Dialog::_handle_input()
{
	Point const orig_hovered_position = _hovered_position;
	bool  const orig_hovered          = _hovered;

	bool seq_number_changed = false;

	_gui.input.for_each_event([&] (Input::Event const &ev) {

		ev.handle_seq_number([&] (Input::Seq_number seq_number) {
			seq_number_changed = true;
			_action.observed_seq_number(seq_number); });

		auto hover_at = [&] (int x, int y)
		{
			_hovered = true;
			_hovered_position = Point(x, y) - _position;
		};

		auto unhover = [&] ()
		{
			_hovered = false;
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

	bool const hover_changed = orig_hovered          != _hovered
	                        || orig_hovered_position != _hovered_position;

	if (hover_changed || seq_number_changed)
		_action.hover_changed();
}

#endif /* _DIALOG_H_ */
