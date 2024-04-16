/*
 * \brief  Runtime state of a child hosted in the runtime subsystem
 * \author Martin Stein
 * \author Norman Feske
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
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
#include <file_vault/types.h>

namespace File_vault { class Child_state; }

class File_vault::Child_state : Noncopyable
{
	private:

		using Registry_element = Registry<Child_state>::Element;

		Registry_element _registry_element;
		Child_name const _start_name;
		Child_name const _binary_name;
		Ram_quota const _initial_ram_quota;
		Cap_quota const _initial_cap_quota;
		Ram_quota _ram_quota { _initial_ram_quota };
		Cap_quota _cap_quota { _initial_cap_quota };

	public:

		Child_state(Registry<Child_state> &registry, Child_name const &start_name,
		            Child_name const &binary_name, Ram_quota ram_quota, Cap_quota cap_quota)
		:
			_registry_element { registry, *this }, _start_name { start_name },
			_binary_name { binary_name }, _initial_ram_quota { ram_quota },
			_initial_cap_quota { cap_quota }
		{ }

		Child_state(Registry<Child_state> &registry, Child_name const &name, Ram_quota ram_quota, Cap_quota cap_quota)
		: Child_state(registry, name, name, ram_quota, cap_quota) { }

		void gen_start_node(Xml_generator &xml, auto const &gen_content) const
		{
			gen_named_node(xml, "start", _start_name, [&] {
				xml.attribute("caps", _cap_quota.value);
				if (_start_name != _binary_name)
					gen_named_node(xml, "binary", _binary_name, [] { });

				gen_named_node(xml, "resource", "RAM", [&] {
					xml.attribute("quantum", Number_of_bytes(_ram_quota.value)); });
				gen_content();
			});
		}

		bool apply_child_state_report(Xml_node const &child)
		{
			bool result = false;
			if (child.attribute_value("name", Child_name()) != _start_name)
				return false;

			if (child.has_sub_node("ram") &&
			    child.sub_node("ram").has_attribute("requested"))
			{
				_ram_quota.value *= 2;
				result = true;
			}
			if (child.has_sub_node("caps") &&
			    child.sub_node("caps").has_attribute("requested"))
			{
				_cap_quota.value += 100;
				result = true;
			}
			return result;
		}

		Ram_quota ram_quota() const { return _ram_quota; }

		Child_name start_name() const { return _start_name; }
};

#endif /* _CHILD_STATE_H_ */
