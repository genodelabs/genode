/*
 * \brief  Global keys handling
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>

/* local includes */
#include "global_keys.h"


Global_keys::Policy *Global_keys::_lookup_policy(char const *key_name)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		if (Genode::strcmp(key_name, Input::key_name((Input::Keycode)i)) == 0)
			return &_policies[i];

	return 0;
}


void Global_keys::apply_config(Session_list &session_list)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		_policies[i].undefine();

	using Genode::Xml_node;
	try {
		Xml_node node = Genode::config()->xml_node().sub_node("global-keys").sub_node("key");

		for (; ; node = node.next("key")) {

			if (!node.has_attribute("name")) {
				PWRN("attribute 'name' missing in <key> config node");
				continue;
			}

			char name[32]; name[0] = 0;
			node.attribute("name").value(name, sizeof(name));

			Policy * policy = _lookup_policy(name);
			if (!policy) {
				PWRN("invalid key name \"%s\"", name);
				continue;
			}

			/* if two policies match, give precedence to policy defined first */
			if (policy->defined())
				continue;

			if (node.has_attribute("operation")) {
				Xml_node::Attribute operation = node.attribute("operation");

				if (operation.has_value("kill")) {
					policy->operation_kill();
					continue;
				} else if (operation.has_value("xray")) {
					policy->operation_xray();
					continue;
				} else {
					char buf[32]; buf[0] = 0;
					operation.value(buf, sizeof(buf));
					PWRN("unknown operation \"%s\" for key %s", buf, name);
				}
				continue;
			}

			if (!node.has_attribute("label")) {
				PWRN("missing 'label' attribute for key %s", name);
				continue;
			}

			/* assign policy to matching client session */
			for (Session *s = session_list.first(); s; s = s->next())
				if (node.attribute("label").has_value(s->label().string()))
					policy->client(s);
		}

	} catch (Xml_node::Nonexistent_sub_node) { }
}

