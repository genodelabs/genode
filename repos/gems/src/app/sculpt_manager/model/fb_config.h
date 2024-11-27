/*
 * \brief  Model for the framebuffer driver configuration
 * \author Norman Feske
 * \date   2024-10-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__FB_CONFIG_H_
#define _MODEL__FB_CONFIG_H_

#include <model/fb_connectors.h>

namespace Sculpt { struct Fb_config; };


struct Sculpt::Fb_config
{
	struct Entry
	{
		using Name       = Fb_connectors::Name;
		using Mode_id    = Fb_connectors::Connector::Mode::Id;
		using Mode_attr  = Fb_connectors::Connector::Mode::Attr;
		using Brightness = Fb_connectors::Brightness;

		bool       defined;
		bool       present;  /* false if imported from config but yet unused */
		Name       name;
		Mode_id    mode_id;
		Mode_attr  mode_attr;
		Brightness brightness;

		static Entry from_connector(Fb_connectors::Connector const &connector)
		{
			Mode_attr mode_attr { };
			Mode_id   mode_id   { };
			connector.with_used_mode([&] (Fb_connectors::Connector::Mode const &mode) {
				mode_attr = mode.attr;
				mode_id   = mode.id; });

			return { .defined    = true,
			         .present    = true,
			         .name       = connector.name,
			         .mode_id    = mode_id,
			         .mode_attr  = mode_attr,
			         .brightness = connector.brightness };
		}

		static Entry from_manual_xml(Xml_node const &node)
		{
			return { .defined    = true,
			         .present    = false,
			         .name       = node.attribute_value("name", Name()),
			         .mode_id    = node.attribute_value("mode", Mode_id()),
			         .mode_attr  = Mode_attr::from_xml(node),
			         .brightness = Brightness::from_xml(node) };
		}

		void generate(Xml_generator &xml) const
		{
			if (!defined)
				return;

			xml.node("connector", [&] {
				xml.attribute("name",   name);
				if (mode_attr.px.valid()) {
					xml.attribute("width",  mode_attr.px.w);
					xml.attribute("height", mode_attr.px.h);
					if (mode_attr.hz)
						xml.attribute("hz", mode_attr.hz);
					if (brightness.defined)
						xml.attribute("brightness", brightness.percent);
					if (mode_id.length() > 1)
						xml.attribute("mode", mode_id);
				} else {
					xml.attribute("enabled", "no");
				}
			});
		}

		bool smaller_than(Entry const &other) const
		{
			return mode_attr.px.count() < other.mode_attr.px.count();
		}
	};

	static constexpr unsigned MAX_ENTRIES = 16;
	Entry _entries[MAX_ENTRIES];

	struct Manual_attr
	{
		Area max_px;   /* upper bound of framebuffer allocation */
		Area px;       /* for vesa_fb */

		static Manual_attr from_xml(Xml_node const &node)
		{
			return { .max_px = { .w = node.attribute_value("max_width", 0u),
			                     .h = node.attribute_value("max_height", 0u) },
			         .px     = Area::from_xml(node) };
		}

		void generate(Xml_generator &xml) const
		{
			if (max_px.w) xml.attribute("max_width",  max_px.w);
			if (max_px.h) xml.attribute("max_height", max_px.h);
			if (px.w)     xml.attribute("width",      px.w);
			if (px.h)     xml.attribute("height",     px.h);
		}
	};

	Manual_attr _manual_attr { };

	unsigned _num_merged = 0;

	bool _known(Fb_connectors::Connector const &connector)
	{
		for (Entry const &entry : _entries)
			if (entry.name == connector.name)
				return true;
		return false;
	}

	void _with_known(Fb_connectors::Connector const &connector, auto const &fn)
	{
		for (Entry &entry : _entries)
			if (entry.name == connector.name)
				fn(entry);
	}

	void _insert_at(unsigned at, Entry const &entry)
	{
		if (at >= MAX_ENTRIES) {
			warning("maximum number of ", MAX_ENTRIES, " fb config entries exeeded");
			return;
		}

		for (unsigned i = MAX_ENTRIES - 1; i > at; i--)
			_entries[i] = _entries[i - 1];

		_entries[at] = entry;
	}

	/*
	 * A new merged connector such that the smallest mode stays in front
	 */
	void _add_unknown_merged(Entry const &new_entry)
	{
		unsigned at = 0;
		for (; at < _num_merged && _entries[at].smaller_than(new_entry); at++);

		_insert_at(at, new_entry);

		if (_num_merged < MAX_ENTRIES)
			_num_merged++;
	}

	void _add_unknown_discrete(Entry const &new_entry)
	{
		unsigned at = 0;
		for (; at < MAX_ENTRIES && _entries[at].defined; at++);

		_insert_at(at, new_entry);
	}

	void import_manual_config(Xml_node const &config)
	{
		_manual_attr = Manual_attr::from_xml(config);

		unsigned count = 0;

		auto add_connectors = [&] (Xml_node const &node)
		{
			node.for_each_sub_node("connector", [&] (Xml_node const &node) {
				Entry const e = Entry::from_manual_xml(node);
				if (!_known(e.name) && count < MAX_ENTRIES) {
					_entries[count] = e;
					count++;
				}
			});
		};

		/* import merged nodes */
		config.with_optional_sub_node("merge", [&] (Xml_node const &merge) {
			add_connectors(merge); });

		_num_merged = count;

		/* import discrete nodes */
		add_connectors(config);

		/* handle case that manual config contains solely discrete items */
		if (count && !_num_merged)
			_num_merged = 1;
	}

	void apply_connectors(Fb_connectors const &connectors)
	{
		/* apply information for connectors known from the manual config */
		connectors.for_each([&] (Fb_connectors::Connector const &conn) {
			_with_known(conn.name, [&] (Entry &e) {

				if (e.present) /* apply config only once */
					return;

				if (!e.mode_attr.px.valid()) { /* switched off by config */
					e.mode_id   = { };
					e.mode_attr = { };
					e.present   = true;
					return;
				}

				conn.with_matching_mode(e.mode_id, e.mode_attr,
					[&] (Fb_connectors::Connector::Mode const &mode) {
						e.mode_id   = mode.id;
						e.mode_attr = mode.attr;
						e.present   = true; });
			});
		});

		/* detect unplugging */
		for (Entry &entry : _entries) {
			bool connected = false;
			connectors.with_connector(entry.name, [&] (auto &) { connected = true; });
			if (!connected)
				entry.present = false;
		}

		connectors._merged.for_each([&] (Fb_connectors::Connector const &conn) {
			if (!_known(conn.name))
				_add_unknown_merged(Entry::from_connector(conn)); });

		connectors._discrete.for_each([&] (Fb_connectors::Connector const &conn) {
			if (!_known(conn.name))
				_add_unknown_discrete(Entry::from_connector(conn)); });
	}

	void _with_entry(Entry::Name const &name, auto const &fn)
	{
		for (Entry &entry : _entries)
			if (entry.name == name)
				fn(entry);
	}

	void select_fb_mode(Fb_connectors::Name                const &conn,
	                    Fb_connectors::Connector::Mode::Id const &mode_id,
	                    Fb_connectors                      const &connectors)
	{
		connectors.with_mode_attr(conn, mode_id, [&] (Entry::Mode_attr const &attr) {
			_with_entry(conn, [&] (Entry &entry) {
				entry.mode_attr = attr;
				entry.mode_id   = mode_id; }); });
	}

	void disable_connector(Fb_connectors::Name const &conn)
	{
		_with_entry(conn, [&] (Entry &entry) {
			entry.mode_attr = { }; });
	}

	void brightness(Fb_connectors::Name const &conn, unsigned percent)
	{
		_with_entry(conn, [&] (Entry &entry) {
			entry.brightness.percent = percent; });
	}

	void _with_idx(Fb_connectors::Name const &conn, auto const &fn) const
	{
		for (unsigned i = 0; i < MAX_ENTRIES; i++)
			if (_entries[i].name == conn && _entries[i].defined) {
				fn(i);
				return;
			}
	}

	void _swap_entries(unsigned i, unsigned j)
	{
		Entry tmp   = _entries[i];
		_entries[i] = _entries[j];
		_entries[j] = tmp;
	}

	/**
	 * Swap connector with next present predecessor
	 */
	void swap_connector(Fb_connectors::Name const &conn)
	{
		_with_idx(conn, [&] (unsigned const conn_idx) {

			if (conn_idx < 1) /* first entry cannot have a predecessor */
				return;

			/* search present predecessor */
			unsigned prev_idx = conn_idx - 1;
			while (prev_idx > 0 && !_entries[prev_idx].present)
				prev_idx--;

			_swap_entries(conn_idx, prev_idx);
		});
	}

	void toggle_merge_discrete(Fb_connectors::Name const &conn)
	{
		_with_idx(conn, [&] (unsigned const idx) {

			if (idx < _num_merged) {

				/*
				 * Turn merged entry into discrete entry.
				 *
				 * There may be (non-present) merge entries following idx.
				 * Bubble up the entry so that it becomes the last merge
				 * entry before turning it into the first discrete entry by
				 * decreasing '_num_merged'.
				 */
				if (_num_merged > 0) {
					for (unsigned i = idx; i < _num_merged - 1; i++)
						_swap_entries(i, i + 1);
					_num_merged--;
				}
			} else {

				/*
				 * Turn discrete entry into merged entry
				 */
				if (_num_merged < MAX_ENTRIES) {
					for (unsigned i = idx; i > _num_merged; i--)
						_swap_entries(i, i - 1);
					_num_merged++;
				}
			}
		});
	}

	struct Merge_info { Entry::Name name; Area px; };

	void with_merge_info(auto const &fn) const
	{
		/* merged screen size and name corresponds to first enabled connector */
		for (unsigned i = 0; i < _num_merged; i++) {
			if (_entries[i].present && _entries[i].mode_attr.px.valid()) {
				fn({ .name = _entries[i].name,
				     .px   = _entries[i].mode_attr.px });
				return;
			}
		}

		/* if all merged connectors are switched off, use name of first present one */
		for (unsigned i = 0; i < _num_merged; i++) {
			if (_entries[i].present) {
				fn({ .name = _entries[i].name, .px = { }});
				return;
			}
		}
	};

	void _gen_merge_node(Xml_generator &xml) const
	{
		with_merge_info([&] (Merge_info const &info) {
			xml.node("merge", [&] {
				xml.attribute("width",  info.px.w);
				xml.attribute("height", info.px.h);
				xml.attribute("name",   info.name);

				for (unsigned i = 0; i < _num_merged; i++)
					_entries[i].generate(xml);
			});
		});
	}

	void generate_managed_fb(Xml_generator &xml) const
	{
		_manual_attr.generate(xml);

		xml.attribute("system", "yes"); /* for screen blanking on suspend */

		xml.node("report", [&] { xml.attribute("connectors", "yes"); });

		_gen_merge_node(xml);

		/* nodes for discrete connectors */
		for (unsigned i = _num_merged; i < MAX_ENTRIES; i++)
			_entries[i].generate(xml);
	}

	void for_each_present_connector(Fb_connectors const &connectors, auto const &fn) const
	{
		for (Entry const &entry : _entries)
			if (entry.defined && entry.present)
				connectors.with_connector(entry.name, fn);
	}

	void for_each_discrete_entry(auto const &fn) const
	{
		for (unsigned i = _num_merged; i < MAX_ENTRIES; i++) {
			Entry const &entry = _entries[i];
			if (entry.defined && entry.present)
				fn(entry);
		}
	}

	unsigned num_present_merged() const
	{
		unsigned count = 0;
		for (unsigned i = 0; i < _num_merged; i++)
			if (_entries[i].defined && _entries[i].present)
				count++;
		return count;
	}
};

#endif /* _MODEL__FB_CONFIG_H_ */
