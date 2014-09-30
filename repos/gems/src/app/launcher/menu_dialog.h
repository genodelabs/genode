/*
 * \brief  Menu dialog
 * \author Norman Feske
 * \date   2014-10-04
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MENU_DIALOG_H_
#define _MENU_DIALOG_H_

/* Genode includes */
#include <timer_session/connection.h>

/* local includes */
#include <fading_dialog.h>
#include <subsystem_manager.h>
#include <context_dialog.h>

namespace Launcher { struct Menu_dialog; }


class Launcher::Menu_dialog : Input_event_handler, Dialog_generator,
                              Hover_handler, Dialog_model,
                              Context_dialog::Response_handler
{
	private:

		typedef String<128> Title;

		struct Element : List<Element>::Element
		{
			Label label;
			Title title;

			bool hovered = false;
			bool touched = false;
			bool running = false;

			Element(Xml_node node)
			:
				label(Decorator::string_attribute(node, "name",  Label(""))),
				title(Decorator::string_attribute(node, "title", Title(label.string())))
			{ }
		};

		List<Element> _elements;

		void _generate_dialog_elements(Xml_generator &xml)
		{
			for (Element const *e = _elements.first(); e; e = e->next()) {

				xml.node("button", [&] () {
					xml.attribute("name", e->label.string());

					if ((e->hovered && !_click_in_progress)
					 || (e->hovered && e->touched))
						xml.attribute("hovered", "yes");

					if (e->running || e->touched)
						xml.attribute("selected", "yes");

					xml.node("label", [&] () {
						xml.attribute("text", e->title.string());
					});
				});
			}
		}

		class Lookup_failed { };

		Element const &_lookup_const(Label const &label) const
		{
			for (Element const *e = _elements.first(); e; e = e->next())
				if (e->label == label)
					return *e;

			throw Lookup_failed();
		}

		Element &_lookup(Label const &label)
		{
			for (Element *e = _elements.first(); e; e = e->next())
				if (e->label == label)
					return *e;

			throw Lookup_failed();
		}

		Fading_dialog::Position _position { 32, 32 };

		Timer::Connection   _timer;
		Subsystem_manager  &_subsystem_manager;
		Nitpicker::Session &_nitpicker;
		Fading_dialog       _dialog;

		Rect _hovered_rect;

		unsigned _key_cnt = 0;
		Label    _clicked;
		bool     _click_in_progress = false;

		Signal_rpc_member<Menu_dialog> _timer_dispatcher;

		Label          _context_subsystem;
		Context_dialog _context_dialog;

		Label _hovered() const
		{
			for (Element const *e = _elements.first(); e; e = e->next())
				if (e->hovered)
					return e->label;

			return Label("");
		}

		bool _running(Label const &label) const
		{
			try { return _lookup_const(label).running; }
			catch (Lookup_failed) { return false; }
		}

		void _running(Label const &label, bool running)
		{
			try { _lookup(label).running = running; }
			catch (Lookup_failed) { }
		}

		void _touch(Label const &label)
		{
			for (Element *e = _elements.first(); e; e = e->next())
				e->touched = (e->label == label);
		}

		/**
		 * Lookup subsystem in config
		 */
		static Xml_node _subsystem(Xml_node config, char const *name)
		{
			Xml_node node = config.sub_node("subsystem");
			for (;; node = node.next("subsystem")) {
				if (node.attribute("name").has_value(name))
					return node;
			}
		}

		void _start(Label const &label)
		{
			try {
				_subsystem_manager.start(_subsystem(config()->xml_node(),
				                                    label.string()));
				_running(label, true);

				dialog_changed();

			} catch (Xml_node::Nonexistent_sub_node) {
				PERR("no subsystem config found for \"%s\"", label.string());
			} catch (Subsystem_manager::Invalid_config) {
				PERR("invalid subsystem configuration for \"%s\"", label.string());
			}
		}

		void _kill(Label const &label)
		{
			_subsystem_manager.kill(label.string());
			_running(label, false);
			dialog_changed();
			_dialog.update();

			_context_dialog.visible(false);
		}

		void _hide(Label const &label)
		{
			_nitpicker.session_control(selector(label.string()),
			                           Nitpicker::Session::SESSION_CONTROL_HIDE);

			_context_dialog.visible(false);
		}

		void _handle_timer(unsigned)
		{
			if (_click_in_progress && _hovered() == _clicked) {

				_touch("");

				Fading_dialog::Position position(_hovered_rect.p2().x(),
				                                 _hovered_rect.p1().y() - 44);
				_context_subsystem = _clicked;
				_context_dialog.position(_position + position);
				_context_dialog.visible(true);
			}

			_click_in_progress = false;
		}

	public:

		Menu_dialog(Server::Entrypoint &ep, Cap_session &cap, Ram_session &ram,
		            Report_rom_slave &report_rom_slave,
		            Subsystem_manager &subsystem_manager,
		            Nitpicker::Session &nitpicker)
		:
			_subsystem_manager(subsystem_manager),
			_nitpicker(nitpicker),
			_dialog(ep, cap, ram, report_rom_slave, "menu_dialog", "menu_hover",
			        *this, *this, *this, *this,
			        _position),
			_timer_dispatcher(ep, *this, &Menu_dialog::_handle_timer),
			_context_dialog(ep, cap, ram, report_rom_slave, *this)
		{
			_timer.sigh(_timer_dispatcher);
		}

		/**
		 * Dialog_generator interface
		 */
		void generate_dialog(Xml_generator &xml) override
		{
			xml.node("frame", [&] () {
				xml.node("vbox", [&] () {
					_generate_dialog_elements(xml);
				});
			});
		}

		Rect _hovered_button_rect(Xml_node hover) const
		{
			Point p(0, 0);

			for (;; hover = hover.sub_node()) {

				p = p + Point(point_attribute(hover));

				if (hover.has_type("button"))
					return Rect(p, area_attribute(hover));

				if (!hover.num_sub_nodes())
					break;
			}

			return Rect();
		}

		/**
		 * Hover_handler interface
		 */
		void hover_changed(Xml_node hover) override
		{
			Label const old_hovered = _hovered();

			for (Element *e = _elements.first(); e; e = e->next())
				e->hovered = false;

			try {
				Xml_node button = hover.sub_node("dialog")
				                       .sub_node("frame")
				                       .sub_node("vbox")
				                       .sub_node("button");

				for (Element *e = _elements.first(); e; e = e->next()) {

					Label const label =
						Decorator::string_attribute(button, "name", Label(""));

					if (e->label == label) {
						e->hovered = true;

						_hovered_rect = _hovered_button_rect(hover);
					}
				}
			} catch (Xml_node::Nonexistent_sub_node) { }

			Label const new_hovered = _hovered();

			if (old_hovered != new_hovered)
				dialog_changed();
		}

		/**
		 * Input_event_handler interface
		 */
		bool handle_input_event(Input::Event const &ev) override
		{
			if (ev.type() == Input::Event::MOTION)
				return true;

			if (ev.type() == Input::Event::PRESS)   _key_cnt++;
			if (ev.type() == Input::Event::RELEASE) _key_cnt--;

			if (ev.type() == Input::Event::PRESS
			 && ev.keycode() == Input::BTN_LEFT
			 && _key_cnt == 1) {

			 	_context_dialog.visible(false);

				Label const hovered = _hovered();

				_click_in_progress = true;
				_clicked = hovered;
				_touch(hovered);

				enum { CONTEXT_DELAY = 500 };

				if (_running(hovered)) {
					_nitpicker.session_control(selector(hovered.string()),
					                           Nitpicker::Session::SESSION_CONTROL_TO_FRONT);
					_nitpicker.session_control(selector(hovered.string()),
					                           Nitpicker::Session::SESSION_CONTROL_SHOW);
					_timer.trigger_once(CONTEXT_DELAY*1000);
				}
			}

			if (ev.type() == Input::Event::RELEASE
			 && _click_in_progress && _key_cnt == 0) {

				Label const hovered = _hovered();

				if (_clicked == hovered) {

					if (!_running(hovered))
						_start(hovered);
				} else {
					_touch("");
				}

				_clicked = Label("");
				_click_in_progress = false;
			}

			return false;
		}

		/**
		 * Context_dialog::Response_handler interface
		 */
		void handle_context_kill() override
		{
			_kill(_context_subsystem);
		}

		/**
		 * Context_dialog::Response_handler interface
		 */
		void handle_context_hide() override
		{
			_hide(_context_subsystem);
		}

		void visible(bool visible)
		{
			_dialog.visible(visible);

			if (!visible)
				_context_dialog.visible(false);
		}

		void update()
		{
			if (_elements.first()) {
				PERR("subsequent updates are not supported");
				return;
			}

			Element *last = nullptr;

			Xml_node subsystems = config()->xml_node();

			subsystems.for_each_sub_node("subsystem",
			                             [&] (Xml_node subsystem)
			{
				Element * const e = new (env()->heap()) Element(subsystem);

				_elements.insert(e, last);
				last = e;
			});

			_dialog.update();
		}
};

#endif /* _MENU_DIALOG_H_ */
