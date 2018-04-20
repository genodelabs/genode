/*
 * \brief  Panel dialog
 * \author Norman Feske
 * \date   2015-10-07
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PANEL_DIALOG_H_
#define _PANEL_DIALOG_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <os/buffered_xml.h>

/* local includes */
#include <fading_dialog.h>
#include <subsystem_manager.h>
#include <context_dialog.h>
#include <menu_dialog.h>

namespace Launcher { struct Panel_dialog; }


class Launcher::Panel_dialog : Input_event_handler, Dialog_generator,
                               Hover_handler, Dialog_model,
                               Context_dialog::Response_handler,
                               Menu_dialog::Response_handler
{
	private:

		typedef String<128> Title;

		struct Element : List<Element>::Element
		{
			Label const label;
			Title const title;

			bool hovered = false;
			bool touched = false;
			bool selected = false;

			Element(Label label, Title title) : label(label), title(title)
			{ }
		};

		Genode::Allocator &_alloc;

		Constructible<Buffered_xml> _config;

		List<Element> _elements;

		Label _focus;

		static char const *_menu_button_label() { return "_menu"; }

		Element _menu_button { _menu_button_label(), "Menu" };

		bool _focused(Element const &e)
		{
			size_t const label_len = strlen(e.label.string());

			if (strcmp(e.label.string(), _focus.string(), label_len))
				return false;

			/*
			 * Even when the strcmp suceeded, the element's label might
			 * not match the focus. E.g., if two subsystems "scout" and
			 * "scoutx" are present the focus of "scoutx" would match both
			 * subsystem labels because the strcmp is limited to the length
			 * of the subsystem label. Hence, we need to make sure that
			 * the focus matched at a separator boundary.
			 */
			char const *char_after_label = _focus.string() + label_len;

			if (*char_after_label == 0 || !strcmp(" -> ", char_after_label, 4))
				return true;

			return false;
		}

		void _generate_dialog_element(Xml_generator &xml, Element const &e)
		{
			xml.node("button", [&] () {
				xml.attribute("name", e.label.string());

				if (&e != &_menu_button)
					xml.attribute("style", "subdued");

				if ((e.hovered && !_click_in_progress)
				 || (e.hovered && e.touched))
					xml.attribute("hovered", "yes");

				if (e.selected || e.touched || _focused(e))
					xml.attribute("selected", "yes");

				xml.node("label", [&] () {
					xml.attribute("text", e.title.string());
				});
			});
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

		Fading_dialog::Position _position { 0, 0 };

		Timer::Connection   _timer;
		Subsystem_manager  &_subsystem_manager;
		Nitpicker::Session &_nitpicker;
		Fading_dialog       _dialog;

		Rect _hovered_rect;

		unsigned _key_cnt = 0;
		Element *_clicked = nullptr;
		bool     _click_in_progress = false;

		Signal_handler<Panel_dialog> _timer_handler;

		Label          _context_subsystem;
		Context_dialog _context_dialog;

		Menu_dialog _menu_dialog;

		Element *_hovered()
		{
			for (Element *e = _elements.first(); e; e = e->next())
				if (e->hovered)
					return e;

			return nullptr;
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
			if (!_config.constructed()) {
				warning("attempt to start subsystem without prior configuration");
				return;
			}

			try {
				Xml_node subsystem = _subsystem(_config->xml(), label.string());
				_subsystem_manager.start(subsystem);

				Title const title = subsystem.attribute_value("title", Title());

				Element *e = new (_alloc) Element(label, title);

				/* find last element of the list */
				Element *at = _elements.first();
				for (; at && at->next(); at = at->next());

				_elements.insert(e, at);

				dialog_changed();

			} catch (Xml_node::Nonexistent_sub_node) {
				Genode::error("no subsystem config found for \"", label, "\"");
			} catch (Subsystem_manager::Invalid_config) {
				Genode::error("invalid subsystem configuration for \"", label, "\"");
			}
		}

		void _kill(Label const &label)
		{
			Element &e = _lookup(label);

			_subsystem_manager.kill(label.string());

			_elements.remove(&e);

			if (_clicked == &e)
				_clicked = nullptr;

			Genode::destroy(_alloc, &e);

			dialog_changed();
			_dialog.update();

			_context_dialog.close();

			_menu_dialog.running(label, false);
		}

		void _hide(Label const &label)
		{
			_nitpicker.session_control(selector(label.string()),
			                           Nitpicker::Session::SESSION_CONTROL_HIDE);

			_context_dialog.close();
		}

		void _open_context_dialog(Label const &label)
		{
			/* reset touch state in each element */
			for (Element *e = _elements.first(); e; e = e->next())
				e->touched = false;

			Fading_dialog::Position position(_hovered_rect.p1().x(),
			                                 _hovered_rect.p2().y());

			_context_subsystem = label;
			_context_dialog.position(_position + position);
			_context_dialog.visible(true);
		}

		void _handle_timer()
		{
			if (_click_in_progress && _clicked && _hovered() == _clicked) {
				_open_context_dialog(_clicked->label);
			}
			_click_in_progress = false;
		}

		void _to_front(Label const &label)
		{
			_nitpicker.session_control(selector(label.string()),
			                           Nitpicker::Session::SESSION_CONTROL_TO_FRONT);
			_nitpicker.session_control(selector(label.string()),
			                           Nitpicker::Session::SESSION_CONTROL_SHOW);
		}

	public:

		Panel_dialog(Env                &env,
		             Genode::Allocator  &alloc,
		             Report_rom_slave   &report_rom_slave,
		             Subsystem_manager  &subsystem_manager,
		             Nitpicker::Session &nitpicker)
		:
			_alloc(alloc),
			_timer(env),
			_subsystem_manager(subsystem_manager),
			_nitpicker(nitpicker),
			_dialog(env, report_rom_slave, "panel_dialog", "panel_hover",
			        *this, *this, *this, *this, _position),
			_timer_handler(env.ep(), *this, &Panel_dialog::_handle_timer),
			_context_dialog(env, report_rom_slave, *this),
			_menu_dialog(env, alloc, report_rom_slave, *this)
		{
			_elements.insert(&_menu_button);
			_timer.sigh(_timer_handler);
		}

		/**
		 * Dialog_generator interface
		 */
		void generate_dialog(Xml_generator &xml) override
		{
			xml.node("hbox", [&] () {
				for (Element const *e = _elements.first(); e; e = e->next())
					_generate_dialog_element(xml, *e);
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
			Element *old_hovered = _hovered();

			for (Element *e = _elements.first(); e; e = e->next())
				e->hovered = false;

			try {
				Xml_node button = hover.sub_node("dialog")
				                       .sub_node("hbox")
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

			Element *new_hovered = _hovered();

			if (old_hovered != new_hovered)
				dialog_changed();
		}

		/**
		 * Input_event_handler interface
		 */
		bool handle_input_event(Input::Event const &ev) override
		{
			if (ev.hover_leave()) {

				/*
				 * Let menu dialog disappear when the panel is unhovered. One
				 * would expect that the user had no chance to select an item
				 * from the menu because when entering the menu, we will no
				 * longer hover the panel. However, the menu disappears slowly.
				 * If the pointer moves over to the menu in a reasonable time,
				 * the visiblity of the menu is re-enabled.
				 */
				_menu_dialog.visible(false);
				_context_dialog.visible(false);
				_menu_button.selected = false;
				_dialog.update();
				return true;
			}

			if (ev.absolute_motion())
				return true;

			if (ev.press())   _key_cnt++;
			if (ev.release()) _key_cnt--;

			if (ev.key_press(Input::BTN_LEFT) && _key_cnt == 1) {

			 	_context_dialog.visible(false);

				Element *hovered = _hovered();

				_click_in_progress = true;
				_clicked = hovered;

				if (!hovered)
					return false;

				hovered->touched = true;

				if (hovered == &_menu_button) {

					/* menu button presses */
					if (_menu_button.selected)
						_menu_dialog.close();
					else
						_menu_dialog.visible(true);

					_menu_button.selected = !_menu_button.selected;
					_dialog.update();
					return false;
				}

				_menu_dialog.close();

				_to_front(hovered->label);

				/*
				 * Open the context dialog after the user keeps pressing the
				 * button for a while.
				 */
				enum { CONTEXT_DELAY = 500 };
				_timer.trigger_once(CONTEXT_DELAY*1000);
			}

			/*
			 * Open context dialog on right click
			 */
			if (ev.key_press(Input::BTN_RIGHT) && _key_cnt == 1) {

				Element *hovered = _hovered();

				if (hovered && hovered != &_menu_button)
					_open_context_dialog(hovered->label);
			}

			if (ev.release() && _click_in_progress && _key_cnt == 0) {

				Element *hovered = _hovered();

				if (hovered)
					hovered->touched = false;

				_clicked = nullptr;
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

		/**
		 * Menu_dialog::Response_handler interface
		 */
		void handle_menu_motion()
		{
			_menu_button.selected = true;
			_dialog.update();
		}

		/**
		 * Menu_dialog::Response_handler interface
		 */
		void handle_menu_leave()
		{
			/* XXX eventually revert the state of the menu button */
			_menu_button.selected = false;

			_dialog.update();

			_menu_dialog.visible(false);
		}

		/**
		 * Menu_dialog::Response_handler interface
		 */
		void handle_selection(Label const &label) override
		{
			/*
			 * If subsystem of the specified label is already running, ignore
			 * the click.
			 */
			bool already_running = false;
			for (Element *e = _elements.first(); e; e = e->next())
				if (e->label == label)
					already_running = true;

			if (already_running) {
				_to_front(label);

			} else {

				_start(label);

				_dialog.update();

				/* propagate running state of subsystem to menu dialog */
				_menu_dialog.running(label, true);
			}

			/* let menu disappear */
			_menu_dialog.close();
		}

		void visible(bool visible)
		{
			_dialog.visible(visible);

			if (!visible)
				_context_dialog.visible(false);
		}

		void kill(Label const &label)
		{
			_kill(label);
		}

		/**
		 * \throw Allocator::Out_of_memory
		 */
		void update(Xml_node config)
		{
			_config.construct(_alloc, config);

			/* populate menu dialog with one item per subsystem */
			_menu_dialog.update(_config->xml());

			/* evaluate configuration */
			_dialog.update();
		}

		void focus_changed(Label const &label)
		{
			_focus = label;

			_dialog.update();
		}

		void focus_next()
		{
			/* find focused element */
			Element *e = _elements.first();

			for (; e && !_focused(*e); e = e->next());

			/* none of our subsystems is focused, start with the first one */
			if (!e)
				e = _elements.first();

			/*
			 * Determine next session in the list, if we reach the end, start
			 * at the beginning (the element right after the menu button.
			 */
			Element *new_focused = e->next() ? e->next() : _menu_button.next();

			if (new_focused)
				_to_front(new_focused->label);
		}
};

#endif /* _PANEL_DIALOG_H_ */
