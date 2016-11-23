/*
 * \brief  Context dialog
 * \author Norman Feske
 * \date   2014-10-04
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CONTEXT_DIALOG_H_
#define _CONTEXT_DIALOG_H_

/* local includes */
#include <fading_dialog.h>

namespace Launcher { struct Context_dialog; }


class Launcher::Context_dialog : Input_event_handler, Dialog_generator,
                                 Hover_handler, Dialog_model
{
	public:

		struct Response_handler
		{
			virtual void handle_context_kill() = 0;
			virtual void handle_context_hide() = 0;
		};

	private:

		struct Element : List<Element>::Element
		{
			Label label;

			bool hovered  = false;
			bool touched  = false;
			bool selected = false;

			Element(char const *label) : label(label) { }
		};

		Label _hovered() const
		{
			for (Element const *e = _elements.first(); e; e = e->next())
				if (e->hovered)
					return e->label;

			return Label("");
		}

		List<Element> _elements;

		void _generate_dialog_elements(Xml_generator &xml)
		{
			for (Element const *e = _elements.first(); e; e = e->next()) {

				xml.node("button", [&] () {
					xml.attribute("name", e->label.string());

					if ((e->hovered && !_click_in_progress)
					 || (e->hovered && e->touched))
						xml.attribute("hovered", "yes");

					if (e->selected || e->touched)
						xml.attribute("selected", "yes");

					xml.node("label", [&] () {
						xml.attribute("text", e->label.string());
					});
				});
			}
		}

		void _touch(Label const &label)
		{
			for (Element *e = _elements.first(); e; e = e->next())
				e->touched = (e->label == label);
		}

		void _reset_hover()
		{
			for (Element *e = _elements.first(); e; e = e->next())
				e->hovered = false;
		}

		Element _hide_button { "Hide" };
		Element _kill_button { "Kill" };

		Fading_dialog _dialog;

		bool _open = false;

		unsigned _key_cnt = 0;
		Label    _clicked;
		bool     _click_in_progress = false;

		Label _subsystem;

		Response_handler &_response_handler;

	public:

		Context_dialog(Env              &env,
		               Report_rom_slave &report_rom_slave,
		               Response_handler &response_handler)
		:
			_dialog(env, report_rom_slave,
			        "context_dialog", "context_hover",
			        *this, *this, *this, *this,
			        Fading_dialog::Position(364, 64)),
			_response_handler(response_handler)
		{
			_elements.insert(&_hide_button);
			_elements.insert(&_kill_button);

			_dialog.update();
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

		/**
		 * Hover_handler interface
		 */
		void hover_changed(Xml_node hover) override
		{
			Label const old_hovered = _hovered();

			_reset_hover();

			try {
				Xml_node button = hover.sub_node("dialog")
				                       .sub_node("frame")
				                       .sub_node("vbox")
				                       .sub_node("button");

				for (Element *e = _elements.first(); e; e = e->next()) {

					Label const label =
						Decorator::string_attribute(button, "name", Label(""));

					if (e->label == label)
						e->hovered = true;
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
			if (ev.type() == Input::Event::MOTION) {

				/*
				 * Re-enable the visibility of the menu if we detect motion
				 * events over the menu. This way, it reappears in situations
				 * where the pointer temporarily leaves the view and returns.
				 */
				if (_open)
					visible(true);

				return true;
			}

			if (ev.type() == Input::Event::LEAVE) {
				visible(false);
				return true;
			}

			if (ev.type() == Input::Event::PRESS)   _key_cnt++;
			if (ev.type() == Input::Event::RELEASE) _key_cnt--;

			if (ev.type() == Input::Event::PRESS
			 && ev.keycode() == Input::BTN_LEFT
			 && _key_cnt == 1) {

				Label const hovered = _hovered();

				_click_in_progress = true;
				_clicked = hovered;
				_touch(hovered);
				dialog_changed();
			}

			if (ev.type() == Input::Event::RELEASE
			 && _click_in_progress && _key_cnt == 0) {

				Label const hovered = _hovered();

				if (_clicked == hovered) {

					if (_kill_button.hovered)
						_response_handler.handle_context_kill();

					if (_hide_button.hovered)
						_response_handler.handle_context_hide();
				} else {
					_touch("");
				}

				_clicked = Label("");
				_click_in_progress = false;
				dialog_changed();
			}

			return false;
		}

		void visible(bool visible)
		{
			if (visible == _dialog.visible())
				return;

			/* reset touch state when (re-)opening the context dialog */
			if (visible) {
				_open = true;
				_touch("");
				_reset_hover();
				dialog_changed();
				_dialog.update();
			}

			_dialog.visible(visible);
		}

		void close()
		{
			_open = false;
			visible(false);
		}

		void position(Fading_dialog::Position position)
		{
			_dialog.position(position);
		}
};

#endif /* _CONTEXT_DIALOG_H_ */
