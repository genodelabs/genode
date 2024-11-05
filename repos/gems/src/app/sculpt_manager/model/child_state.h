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
#include <util/xml_node.h>
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
			Start_name name;
			Priority   priority;
			unsigned   cpu_quota;

			Affinity::Location location;

			struct Initial { Ram_quota ram; Cap_quota caps; } initial;
			struct Max     { Ram_quota ram; Cap_quota caps; } max;

			static constexpr Max DEFAULT_MAX = { .ram  = { 256*1024*1024 },
			                                     .caps = { 5000 } };
		};

	private:

		Registry<Child_state>::Element _element;

		Attr const _attr;

		static Attr _init_attr(Attr const attr)
		{
			Attr result = attr;
			if (!result.max.ram .value) result.max.ram  = Attr::DEFAULT_MAX.ram;
			if (!result.max.caps.value) result.max.caps = Attr::DEFAULT_MAX.caps;
			return result;
		}

		Ram_quota _ram_quota = _attr.initial.ram;
		Cap_quota _cap_quota = _attr.initial.caps;

		struct Warned_once { bool ram, caps; } _warned_once { };

		Version _version { 0 };

		static bool _location_valid(Attr const attr)
		{
			return attr.location.width() != 0 && attr.location.height() != 0;
		}

	public:

		Child_state(Registry<Child_state> &registry, Attr const attr)
		: _element(registry, *this), _attr(_init_attr(attr)) { }

		Child_state(Registry<Child_state> &registry, auto const &name,
		            Priority priority, Ram_quota initial_ram, Cap_quota initial_caps)
		:
			Child_state(registry, { .name      = name,
			                        .priority  = priority,
			                        .cpu_quota = 0,
			                        .location  = { },
			                        .initial   = { initial_ram, initial_caps },
			                        .max       = { } })
		{ }

		void trigger_restart()
		{
			_version.value++;
			_ram_quota = _attr.initial.ram;
			_cap_quota = _attr.initial.caps;
		}

		void gen_start_node_version(Xml_generator &xml) const
		{
			if (_version.value)
				xml.attribute("version", _version.value);
		}

		void gen_start_node_content(Xml_generator &xml) const
		{
			xml.attribute("name", _attr.name);

			gen_start_node_version(xml);

			xml.attribute("caps", _cap_quota.value);
			xml.attribute("priority", (int)_attr.priority);

			gen_named_node(xml, "resource", "RAM", [&] {
				Number_of_bytes const bytes(_ram_quota.value);
				xml.attribute("quantum", String<64>(bytes)); });

			if (_attr.cpu_quota)
				gen_named_node(xml, "resource", "CPU", [&] {
					xml.attribute("quantum", _attr.cpu_quota); });

			if (_location_valid(_attr))
				xml.node("affinity", [&] {
					xml.attribute("xpos",   _attr.location.xpos());
					xml.attribute("ypos",   _attr.location.ypos());
					xml.attribute("width",  _attr.location.width());
					xml.attribute("height", _attr.location.height());
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
		bool apply_child_state_report(Xml_node child)
		{
			bool result = false;

			if (child.attribute_value("name", Start_name()) != _attr.name)
				return false;

			auto upgrade = [&] (auto const &msg, auto &quota, auto &max_quota, bool &warned_once)
			{
				if (quota.value == max_quota.value) {
					if (!warned_once)
						warning(msg, " consumption of ", _attr.name, " exceeeded maximum of ", max_quota);
					warned_once = true;
				} else {
					quota.value = min(quota.value*2, max_quota.value);
					result = true;
				}
			};

			if (child.has_sub_node("ram") && child.sub_node("ram").has_attribute("requested"))
				upgrade("RAM",  _ram_quota, _attr.max.ram,  _warned_once.ram);

			if (child.has_sub_node("caps") && child.sub_node("caps").has_attribute("requested"))
				upgrade("caps", _cap_quota, _attr.max.caps, _warned_once.caps);

			bool const responsive = (child.attribute_value("skipped_heartbeats", 0U) <= 4);
			if (!responsive) {
				trigger_restart();
				result = true;
			}

			return result;
		}

		Ram_quota ram_quota() const { return _ram_quota; }

		Start_name name() const { return _attr.name; }
};

#endif /* _MODEL__CHILD_STATE_H_ */
