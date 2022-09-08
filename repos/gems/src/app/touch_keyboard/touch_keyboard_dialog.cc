/*
 * \brief  Touch-screen keyboard dialog
 * \author Norman Feske
 * \date   2022-01-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <touch_keyboard_dialog.h>

using namespace Touch_keyboard;


void Dialog::produce_xml(Xml_generator &xml)
{
	auto gen_key = [&] (Key const &key)
	{
		xml.node("vbox", [&] () {
			xml.attribute("name", key._id);

			xml.node("button", [&] () {
				bool const selected = (_emit_on_release.length() > 1)
				                   && (key.emit == _emit_on_release);
				if (selected)
					xml.attribute("selected", "yes");

				if (key.map.length() > 1)
					xml.attribute("style", "unimportant");

				if (key.label.length() == 1)
					xml.attribute("style", "invisible");

				xml.node("vbox", [&] () {
					xml.node("label", [&] () {
						xml.attribute("name", "spacer");
						unsigned const min_ex = key.min_ex
						                      ? key.min_ex
						                      : _default_key_min_ex;
						if (min_ex)
							xml.attribute("min_ex", min_ex);
					});
					xml.node("label", [&] () {
						xml.attribute("name", "label");
						xml.attribute("text", key.label);
						if (key.small)
							xml.attribute("font", "annotation/regular");
					});
				});
			});
			xml.node("label", [&] () {
				xml.attribute("name", "spacer");
				xml.attribute("font", "annotation/regular");
				xml.attribute("text", "");
			});
		});
	};

	auto gen_row = [&] (Row const &row)
	{
		xml.node("hbox", [&] () {
			xml.attribute("name", row._id);
			row.keys.for_each([&] (Key const &key) {
				gen_key(key); }); });
	};

	auto gen_map = [&] (Map const &map)
	{
		if (map.name != _current_map)
			return;

		xml.node("vbox", [&] () {
			map.rows.for_each([&] (Row const &row) {
				gen_row(row); }); });
	};

	xml.node("frame", [&] () {
		_maps.for_each([&] (Map const &map) {
			gen_map(map);
		});
	});
}


void Dialog::handle_input_event(Input::Seq_number curr_seq, Input::Event const &event)
{
	if (event.touch())
		_clicked_seq_number.construct(curr_seq);

	if (_emit_on_release.length() > 1 && event.touch_release()) {
		_event_emitter.emit_characters(_emit_on_release);
		_emit_on_release = { };
		rom_session.trigger_update();
	}
}


void Dialog::handle_hover(Input::Seq_number seq, Xml_node const &dialog)
{
	Row::Id hovered_row_id { };
	Key::Id hovered_key_id { };

	dialog.with_optional_sub_node("frame", [&] (Xml_node const &frame) {
		frame.with_optional_sub_node("vbox", [&] (Xml_node const &vbox) {
			vbox.with_optional_sub_node("hbox", [&] (Xml_node const &hbox) {
				hbox.with_optional_sub_node("vbox", [&] (Xml_node const &button) {
					hovered_row_id = hbox  .attribute_value("name", Row::Id());
					hovered_key_id = button.attribute_value("name", Key::Id());
				});
			});
		});
	});

	_maps.for_each([&] (Map const &map) {
		if (map.name != _current_map)
			return;

		map.rows.for_each([&] (Row const &row) {
			if (row._id != hovered_row_id)
				return;

			row.keys.for_each([&] (Key const &key) {
				if (key._id != hovered_key_id)
					return;

				if (_clicked_seq_number.constructed()) {
					if (seq.value >= _clicked_seq_number->value) {
						_emit_on_release = key.emit;
						_clicked_seq_number.destruct();

						if (key.map.length() > 1)
							_current_map = key.map;

						rom_session.trigger_update();
					}
				}
			});
		});
	});
}


Dialog::Dialog(Entrypoint &ep, Ram_allocator &ram, Region_map &rm,
               Allocator &alloc, Event_emitter &event_emitter)
:
	Xml_producer("dialog"),
	rom_session(ep, ram, rm, *this),
	_alloc(alloc), _event_emitter(event_emitter)
{ }
