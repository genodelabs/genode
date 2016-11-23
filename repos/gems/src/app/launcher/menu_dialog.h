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
                              Hover_handler, Dialog_model
{
	public:

		struct Response_handler
		{
			virtual void handle_selection(Label const &) = 0;
			virtual void handle_menu_leave() = 0;
			virtual void handle_menu_motion() = 0;
		};

	private:

		Response_handler &_response_handler;

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

					if ((e->hovered)
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

		Fading_dialog::Position _position { 0 - 4, 28 - 4 };

		Fading_dialog _dialog;

		Rect _hovered_rect;

		bool _open = false;

		unsigned _key_cnt = 0;

		Label _hovered() const
		{
			for (Element const *e = _elements.first(); e; e = e->next())
				if (e->hovered)
					return e->label;

			return Label("");
		}

	public:

		Menu_dialog(Env              &env,
		            Report_rom_slave &report_rom_slave,
		            Response_handler &response_handler)
		:
			_response_handler(response_handler),
			_dialog(env, report_rom_slave, "menu_dialog", "menu_hover",
			        *this, *this, *this, *this, _position)
		{ }

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
			if (ev.type() == Input::Event::LEAVE) {
				_response_handler.handle_menu_leave();
				return false;
			}

			if (ev.type() == Input::Event::MOTION) {

				_response_handler.handle_menu_motion();

				/*
				 * Re-enable the visibility of the menu if we detect motion
				 * events over the menu. This way, it reappears in situations
				 * where the pointer temporarily leaves the view and returns.
				 */
				if (_open)
					visible(true);

				return true;
			}

			if (ev.type() == Input::Event::PRESS)   _key_cnt++;
			if (ev.type() == Input::Event::RELEASE) _key_cnt--;

			if (ev.type() == Input::Event::PRESS
			 && ev.keycode() == Input::BTN_LEFT
			 && _key_cnt == 1) {

			 	_response_handler.handle_selection(_hovered());
			}

			return false;
		}

		void visible(bool visible)
		{
			if (visible == _dialog.visible())
				return;

			_dialog.visible(visible);

			if (visible)
				_open = true;
		}

		void close()
		{
			_open = false;
			visible(false);
		}

		void running(Label const &label, bool running)
		{
			for (Element *e = _elements.first(); e; e = e->next())
				if (e->label == label)
					e->running = running;

			_dialog.update();
		}

		void update(Xml_node subsystems)
		{
			if (_elements.first()) {
				Genode::error("subsequent updates are not supported");
				return;
			}

			Element *last = nullptr;

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
