/*
 * \brief  Representation of nitpicker's <capture> configuration
 * \author Norman Feske
 * \date   2024-10-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__PANORAMA_CONFIG_H_
#define _MODEL__PANORAMA_CONFIG_H_

#include <types.h>
#include <model/fb_config.h>

namespace Sculpt { struct Panorama_config; };


struct Sculpt::Panorama_config
{
	struct Entry
	{
		using Name = Fb_config::Entry::Name;

		Name name;
		Rect rect;

		bool operator != (Entry const &other) const
		{
			return (name != other.name) || (rect != other.rect);
		}

		void gen_policy(Xml_generator &xml) const
		{
			xml.node("policy", [&] {
				xml.attribute("label_suffix", name);
				xml.attribute("xpos",   rect.x1());
				xml.attribute("ypos",   rect.y1());
				xml.attribute("width",  rect.w());
				xml.attribute("height", rect.h());
			});
		}
	};

	static constexpr unsigned MAX_ENTRIES = 16;

	Entry _entries[MAX_ENTRIES] { };

	unsigned _num_entries = 0;

	Panorama_config() { };

	Panorama_config(Fb_config const &fb_config)
	{
		int xpos = 0;

		auto append = [&] (auto const &name, auto const area)
		{
			if (_num_entries == MAX_ENTRIES)
				return;

			_entries[_num_entries] = Entry {
				.name = name,
				.rect = { .at = { .x = xpos, .y = 0 }, .area = area } };

			_num_entries++;
			xpos += area.w;
		};

		fb_config.with_merge_info([&] (Fb_config::Merge_info const &info) {
			append(info.name, info.px); });

		fb_config.for_each_discrete_entry([&] (Fb_config::Entry const &entry) {
			append(entry.name, entry.mode_attr.px); });
	}

	void gen_policy_entries(Xml_generator &xml) const
	{
		for (unsigned i = 0; i < _num_entries; i++)
			_entries[i].gen_policy(xml);
	}

	bool operator != (Panorama_config const &other) const
	{
		if (other._num_entries != _num_entries)
			return true;

		for (unsigned i = 0; i < _num_entries; i++)
			if (other._entries[i] != _entries[i])
				return true;

		return false;
	}
};

#endif /* _MODEL__PANORAMA_CONFIG_H_ */
