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

#ifndef _CHILD_STATE_H_
#define _CHILD_STATE_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/noncopyable.h>
#include <util/string.h>
#include <base/registry.h>
#include <base/quota_guard.h>

/* local includes */
#include "types.h"

namespace Touch_keyboard { struct Child_state; }

struct Touch_keyboard::Child_state : Noncopyable
{
	private:

		using Start_name = String<128>;

		Registry<Child_state>::Element _element;

		Start_name const _name;

		Ram_quota const _initial_ram_quota;
		Cap_quota const _initial_cap_quota;

		Ram_quota _ram_quota = _initial_ram_quota;
		Cap_quota _cap_quota = _initial_cap_quota;

		struct Version { unsigned value; } _version { 0 };

	public:

		/**
		 * Constructor
		 *
		 * \param ram_quota  initial RAM quota
		 * \param cap_quota  initial capability quota
		 */
		Child_state(Registry<Child_state> &registry, Start_name const &name,
		            Ram_quota ram_quota, Cap_quota cap_quota)
		:
			_element(registry, *this),
			_name(name),
			_initial_ram_quota(ram_quota), _initial_cap_quota(cap_quota)
		{ }

		void trigger_restart()
		{
			_version.value++;
			_ram_quota = _initial_ram_quota;
			_cap_quota = _initial_cap_quota;
		}

		void gen_start_node_version(Xml_generator &xml) const
		{
			if (_version.value)
				xml.attribute("version", _version.value);
		}

		void gen_start_node_content(Xml_generator &xml) const
		{
			xml.attribute("name", _name);

			gen_start_node_version(xml);

			xml.attribute("caps", _cap_quota.value);
			xml.node("resource", [&] () {
				xml.attribute("name", "RAM");
				Number_of_bytes const bytes(_ram_quota.value);
				xml.attribute("quantum", String<64>(bytes)); });
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

			if (child.attribute_value("name", Start_name()) != _name)
				return false;

			if (child.has_sub_node("ram") && child.sub_node("ram").has_attribute("requested")) {
				_ram_quota.value *= 2;
				result = true;
			}

			if (child.has_sub_node("caps") && child.sub_node("caps").has_attribute("requested")) {
				_cap_quota.value += 100;
				result = true;
			}

			return result;
		}

		Ram_quota ram_quota() const { return _ram_quota; }

		Start_name name() const { return _name; }
};

#endif /* _CHILD_STATE_H_ */
