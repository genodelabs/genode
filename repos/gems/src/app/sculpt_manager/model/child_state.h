/*
 * \brief  Runtime state of a child hosted in the runtime subsystem
 * \author Norman Feske
 * \date   2018-09-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__CHILD_STATE_H_
#define _MODEL__CHILD_STATE_H_

/* Genode includes */
#include <base/registry.h>

/* local includes */
#include "types.h"
#include <xml.h>

namespace Sculpt { struct Child_state; }

struct Sculpt::Child_state : Noncopyable
{
	public:

		struct Version { unsigned value; };

		struct Attr
		{
			Child_name  name;
			Binary_name binary;
			Priority    priority;

			Affinity::Location location;

			struct Initial { Ram_quota ram; Cap_quota caps; } initial;
			struct Max     { Ram_quota ram; Cap_quota caps; } max;

			static constexpr Max DEFAULT_MAX = { .ram  = { 256*1024*1024 },
			                                     .caps = { 5000 } };
		};

		Attr const attr;

	private:

		Registry<Child_state>::Element _element;

		static Attr _init_attr(Attr const attr)
		{
			Attr result = attr;
			if (!result.max.ram .value) result.max.ram  = Attr::DEFAULT_MAX.ram;
			if (!result.max.caps.value) result.max.caps = Attr::DEFAULT_MAX.caps;
			return result;
		}

		Ram_quota _ram_quota = attr.initial.ram;
		Cap_quota _cap_quota = attr.initial.caps;

		struct Warned_once { bool ram, caps; } _warned_once { };

		Version _version { 0 };

		static bool _location_valid(Attr const attr)
		{
			return attr.location.width() != 0 && attr.location.height() != 0;
		}

	public:

		Child_state(Registry<Child_state> &registry, Attr const attr)
		: attr(_init_attr(attr)), _element(registry, *this) { }

		Child_state(Registry<Child_state> &registry, Priority priority,
		            Child_name const &name, Binary_name const &binary,
		            Ram_quota initial_ram, Cap_quota initial_caps)
		:
			Child_state(registry, { .name      = name,
			                        .binary    = binary,
			                        .priority  = priority,
			                        .location  = { },
			                        .initial   = { initial_ram, initial_caps },
			                        .max       = { } })
		{ }

		void trigger_restart()
		{
			_version.value++;
			_ram_quota = attr.initial.ram;
			_cap_quota = attr.initial.caps;
		}

		void gen_start_node_version(Generator &g) const
		{
			if (_version.value)
				g.attribute("version", _version.value);
		}

		void gen_start_node_content(Generator &g) const
		{
			g.attribute("name", attr.name);

			gen_start_node_version(g);

			g.attribute("caps", _cap_quota.value);
			g.attribute("priority", (int)attr.priority);

			if (attr.binary != attr.name)
				gen_named_node(g, "binary", attr.binary);

			gen_named_node(g, "resource", "RAM", [&] {
				Number_of_bytes const bytes(_ram_quota.value);
				g.attribute("quantum", String<64>(bytes)); });

			if (_location_valid(attr))
				g.node("affinity", [&] {
					g.attribute("xpos",   attr.location.xpos());
					g.attribute("ypos",   attr.location.ypos());
					g.attribute("width",  attr.location.width());
					g.attribute("height", attr.location.height());
				});
		}

		void gen_child_node_content(Generator &g) const
		{
			gen_child_attr(g, attr.name, attr.binary, _cap_quota, _ram_quota,
			               attr.priority);

			if (_version.value)
				g.attribute("version", _version.value);

			if (_location_valid(attr))
				g.node("affinity", [&] {
					g.attribute("xpos",   attr.location.xpos());
					g.attribute("ypos",   attr.location.ypos());
					g.attribute("width",  attr.location.width());
					g.attribute("height", attr.location.height());
				});
		}

		/**
		 * Adapt runtime state information to the child
		 *
		 * This method responds to RAM and cap-resource requests by increasing
		 * the resource quotas as needed.
		 *
		 * \param  child  child node of the runtime'r state report
		 * \return true if runtime must be reconfigured so that the changes
		 *         can take effect
		 */
		bool apply_child_state_report(Node const &child)
		{
			bool result = false;

			if (child.attribute_value("name", Start_name()) != attr.name)
				return false;

			auto upgrade = [&] (auto const &msg, auto &quota, auto &max_quota, bool &warned_once)
			{
				if (quota.value == max_quota.value) {
					if (!warned_once)
						warning(msg, " consumption of ", attr.name, " exceeeded maximum of ", max_quota);
					warned_once = true;
				} else {
					quota.value = min(quota.value*2, max_quota.value);
					result = true;
				}
			};

			child.with_optional_sub_node("ram", [&] (Node const &node) {
				if (node.has_attribute("requested"))
					upgrade("RAM",  _ram_quota, attr.max.ram,  _warned_once.ram); });

			child.with_optional_sub_node("caps", [&] (Node const &node) {
				if (node.has_attribute("requested"))
					upgrade("caps", _cap_quota, attr.max.caps, _warned_once.caps); });

			bool const responsive = (child.attribute_value("skipped_heartbeats", 0U) <= 4);
			if (!responsive) {
				trigger_restart();
				result = true;
			}

			return result;
		}

		Ram_quota ram_quota() const { return _ram_quota; }
};

#endif /* _MODEL__CHILD_STATE_H_ */
