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

#include <base/allocator.h>
#include <util/list_model.h>
#include <dialog/widgets.h>

namespace Dialog { struct Touch_keyboard_widget; }


struct Dialog::Touch_keyboard_widget : Widget<Vbox>
{
	public:

		using Emit = String<8>;

	private:

		Allocator &_alloc;

		struct Default_key_min_ex { unsigned value; };

		Default_key_min_ex _default_key_min_ex { 0 };

		static Id _id_attr(Xml_node const &node)
		{
			return { node.attribute_value("id", Id::Value { }) };
		}

		struct Key : Widget<Vbox>
		{
			Id const _id;

			using Text = String<8>;
			Text text { };

			Emit emit { };

			using Map = String<8>;
			Map map { };

			unsigned min_ex = 0;

			bool small = false;

			Key(Id id) : _id(id) { }

			bool matches(Xml_node const &node) const { return _id == _id_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "key"; }

			Hosted<Vbox, Action_button> _button { Id { } };

			void update(Xml_node const &key) {

				text   = { };
				emit   = { };
				map    = key.attribute_value("map", Map());
				min_ex = key.attribute_value("min_ex", 0U);
				small  = key.attribute_value("small", false);

				if (key.has_attribute("char")) {
					text = key.attribute_value("char", Text());
					emit = Emit(Xml_unquoted(text));
				}

				if (key.has_attribute("code")) {
					Codepoint c { key.attribute_value("code", 0U) };
					emit = Emit(c);
					text = emit;
				}

				if (key.has_attribute("label"))
					text = key.attribute_value("label", Text());
			}

			struct Attr { Default_key_min_ex default_key_min_ex; };

			void view(Scope<Vbox> &s, Attr const &attr) const;

			void click(Clicked_at const &at, auto const &fn)
			{
				_button.propagate(at, [&] { /* drive selected state */ });
				fn(*this);
			}
		};

		struct Row : Widget<Hbox>
		{
			Allocator &_alloc;

			Id const _id;

			struct Hosted_key : List_model<Hosted_key>::Element, Hosted<Hbox, Key>
			{
				using Hosted::Hosted;
			};

			List_model<Hosted_key> keys { };

			Row(Allocator &alloc, Id const id) : _alloc(alloc), _id(id) { }

			bool matches(Xml_node const &node) const { return _id == _id_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "row"; }

			void update(Xml_node const &row)
			{
				keys.update_from_xml(row,

					/* create */
					[&] (Xml_node const &node) -> Hosted_key & {
						Id const id = _id_attr(node);
						return *new (_alloc) Hosted_key { id, id }; },

					/* destroy */
					[&] (Hosted_key &key) { destroy(_alloc, &key); },

					/* update */
					[&] (Hosted_key &key, Xml_node const &node) { key.update(node); });
			}

			struct Attr { Default_key_min_ex default_key_min_ex; };

			void view(Scope<Hbox> &, Attr const &) const;

			void click(Clicked_at const &at, auto const &fn)
			{
				keys.for_each([&] (Hosted_key &key) {
					key.propagate(at, fn); });
			}
		};

		struct Map : List_model<Map>::Element
		{
			Allocator &_alloc;

			using Name = String<16>;
			Name const name;

			static Name name_attr(Xml_node const &node) {
				return node.attribute_value("name", Name()); }

			struct Hosted_row : List_model<Hosted_row>::Element, Hosted<Vbox, Row>
			{
				using Hosted::Hosted;
			};

			List_model<Hosted_row> rows { };

			Map(Allocator &alloc, Name name) : _alloc(alloc), name(name) { }

			bool matches(Xml_node const &node) const { return name == name_attr(node); }

			static bool type_matches(Xml_node const &node) { return node.type() == "map"; }

			void update(Xml_node const &map)
			{
				rows.update_from_xml(map,

					/* create */
					[&] (Xml_node const &node) -> Hosted_row & {
						Id const id = _id_attr(node);
						return *new (_alloc) Hosted_row { id, _alloc, id }; },

					/* destroy */
					[&] (Hosted_row &row) { destroy(_alloc, &row); },

					/* update */
					[&] (Hosted_row &row, Xml_node const &node) { row.update(node); });
			}
		};

		List_model<Map> _maps { };

		Map::Name _current_map = "lower";

		Emit _emit_on_clack { };

		static void _with_current_map(auto &keyboard, auto const &fn)
		{
			keyboard._maps.for_each([&] (auto &map) {
				if (map.name == keyboard._current_map)
					fn(map); });
		}

	public:

		Touch_keyboard_widget(Allocator &alloc) : _alloc(alloc) { }

		void configure(Xml_node const &config)
		{
			_default_key_min_ex = { config.attribute_value("key_min_ex", 0U) };

			_maps.update_from_xml(config,

				/* create */
				[&] (Xml_node const &node) -> Map & {
					return *new (_alloc) Map(_alloc, Map::name_attr(node)); },

				/* destroy */
				[&] (Map &map) { destroy(_alloc, &map); },

				/* update */
				[&] (Map &map, Xml_node const &node) { map.update(node); });
		}

		void view(Scope<Vbox> &s) const;

		void click(Clicked_at const &);

		void clack(Clacked_at const &, auto const &fn)
		{
			if (_emit_on_clack.length() > 1) {
				fn(_emit_on_clack);
				_emit_on_clack = { };
			}
		}

};

#endif /* _TOUCH_KEYBOARD_DIALOG_H_ */
