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

#ifndef _TOUCH_KEYBOARD_DIALOG_H_
#define _TOUCH_KEYBOARD_DIALOG_H_

/* Genode includes */
#include <util/list_model.h>
#include <base/component.h>
#include <base/session_object.h>
#include <sandbox/sandbox.h>
#include <os/dynamic_rom_session.h>
#include <input/event.h>

/* local includes */
#include <report.h>
#include <types.h>

namespace Touch_keyboard { struct Dialog; }


struct Touch_keyboard::Dialog : private Dynamic_rom_session::Xml_producer
{
	public:

		Dynamic_rom_session rom_session;

		using Emit = String<8>;

		struct Event_emitter : Interface
		{
			virtual void emit_characters(Emit const &) = 0;
		};

	private:

		Allocator &_alloc;

		Event_emitter &_event_emitter;

		unsigned _default_key_min_ex = 0;

		struct Key : List_model<Key>::Element
		{
			using Id = String<8>;
			Id const _id;

			static Id id_attr(Xml_node const &node) {
				return node.attribute_value("id", Id()); }

			using Label = String<8>;
			Label label { };

			Emit emit { };

			using Map = String<8>;
			Map map { };

			unsigned min_ex = 0;

			bool small = false;

			Key(Id id) : _id(id) { }

			bool matches(Xml_node const &node) const { return _id == id_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "key"; }

			void update(Xml_node const &key) {

				label  = { };
				emit   = { };
				map    = key.attribute_value("map", Map());
				min_ex = key.attribute_value("min_ex", 0U);
				small  = key.attribute_value("small", false);

				if (key.has_attribute("char")) {
					label = key.attribute_value("char", Label());
					emit  = Emit(Xml_unquoted(label));
				}

				if (key.has_attribute("code")) {
					Codepoint c { key.attribute_value("code", 0U) };
					emit  = Emit(c);
					label = emit;
				}

				if (key.has_attribute("label"))
					label = key.attribute_value("label", Label());
			}
		};

		struct Row : List_model<Row>::Element
		{
			Allocator &_alloc;

			using Id = String<8>;
			Id const _id;

			static Id id_attr(Xml_node const &node) {
				return node.attribute_value("id", Id()); }

			List_model<Key> keys { };

			Row(Allocator &alloc, Id id) : _alloc(alloc), _id(id) { }

			bool matches(Xml_node const &node) const { return _id == id_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "row"; }

			void update(Xml_node const &row)
			{
				keys.update_from_xml(row,

					/* create */
					[&] (Xml_node const &node) -> Key & {
						return *new (_alloc) Key(Key::id_attr(node)); },

					/* destroy */
					[&] (Key &key) { destroy(_alloc, &key); },

					/* update */
					[&] (Key &key, Xml_node const &node) { key.update(node); });
			}
		};

		struct Map : List_model<Map>::Element
		{
			Allocator &_alloc;

			using Name = String<16>;
			Name const name;

			static Name name_attr(Xml_node const &node) {
				return node.attribute_value("name", Name()); }

			List_model<Row> rows { };

			Map(Allocator &alloc, Name name) : _alloc(alloc), name(name) { }

			bool matches(Xml_node const &node) const { return name == name_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "map"; }

			void update(Xml_node const &map)
			{
				rows.update_from_xml(map,

					/* create */
					[&] (Xml_node const &node) -> Row & {
						return *new (_alloc) Row(_alloc, Row::id_attr(node)); },

					/* destroy */
					[&] (Row &row) { destroy(_alloc, &row); },

					/* update */
					[&] (Row &row, Xml_node const &node) { row.update(node); });
			}
		};

		List_model<Map> _maps { };

		Map::Name _current_map = "lower";

		Constructible<Input::Seq_number> _clicked_seq_number { };

		Emit _emit_on_release { };

		void produce_xml(Xml_generator &xml) override;

	public:

		Dialog(Entrypoint &, Ram_allocator &, Region_map &, Allocator &,
		       Event_emitter &);

		void configure(Xml_node const &config)
		{
			_default_key_min_ex = config.attribute_value("key_min_ex", 0U);

			_maps.update_from_xml(config,

				/* create */
				[&] (Xml_node const &node) -> Map & {
					return *new (_alloc) Map(_alloc, Map::name_attr(node)); },

				/* destroy */
				[&] (Map &map) { destroy(_alloc, &map); },

				/* update */
				[&] (Map &map, Xml_node const &node) { map.update(node); });
		}

		void handle_input_event(Input::Seq_number curr_seq, Input::Event const &);

		void handle_hover(Input::Seq_number, Xml_node const &hover);
};

#endif /* _TOUCH_KEYBOARD_DIALOG_H_ */
