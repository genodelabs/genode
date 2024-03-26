/*
 * \brief  Cached information about available deploy presets
 * \author Norman Feske
 * \date   2022-01-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__PRESETS_H_
#define _MODEL__PRESETS_H_

/* Genode includes */
#include <util/list_model.h>
#include <util/dictionary.h>

/* local includes */
#include <types.h>

namespace Sculpt { class Presets; }


class Sculpt::Presets : public Noncopyable
{
	public:

		struct Info : Noncopyable
		{
			using Name = String<64>;
			using Text = String<128>;

			Name const name;
			Text const text;

			static Text _info_text(Xml_node const &node)
			{
				Text result { };
				node.with_optional_sub_node("config", [&] (Xml_node const &config) {
					result = config.attribute_value("info", Text()); });
				return result;
			}

			Info(Xml_node const &node)
			:
				name(node.attribute_value("name", Path())),
				text(_info_text(node))
			{ }
		};

	private:

		Allocator &_alloc;

		unsigned _count = 0;

		struct Preset;

		using Dict = Dictionary<Preset, Path>;

		struct Preset : Dict::Element, List_model<Preset>::Element
		{
			Info const info;

			bool matches(Xml_node const &node) const
			{
				return node.attribute_value("name", Path()) == name;
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("file");
			}

			Preset(Dict &dict, Xml_node const &node)
			:
				Dict::Element(dict, node.attribute_value("name", Path())),
				info(node)
			{ }
		};

		Dict _sorted { };

		List_model<Preset> _presets { };

	public:

		Presets(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node const &presets)
		{
			_presets.update_from_xml(presets,

				/* create */
				[&] (Xml_node const &node) -> Preset & {
					return *new (_alloc) Preset(_sorted, node); },

				/* destroy */
				[&] (Preset &e) { destroy(_alloc, &e); },

				/* update */
				[&] (Preset &, Xml_node) { }
			);

			_count = 0;
			_presets.for_each([&] (Preset const &) { _count++; });
		}

		void for_each(auto const &fn) const
		{
			_sorted.for_each([&] (Preset const &preset) { fn(preset.info); });
		}

		bool available() const { return _count > 0; };
};

#endif /* _MODEL__PRESETS_H_ */
