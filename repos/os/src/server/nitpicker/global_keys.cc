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

/* local includes */
#include "global_keys.h"


Global_keys::Policy *Global_keys::_lookup_policy(char const *key_name)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		if (Genode::strcmp(key_name, Input::key_name((Input::Keycode)i)) == 0)
			return &_policies[i];

	return 0;
}


void Global_keys::apply_config(Xml_node config, Session_list &session_list)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		_policies[i] = Policy();

	char const *node_type = "global-key";

	try {
		Xml_node node = config.sub_node(node_type);

		for (; ; node = node.next(node_type)) {

			if (!node.has_attribute("name")) {
				Genode::warning("attribute 'name' missing in <global-key> config node");
				continue;
			}

			typedef Genode::String<32> Name;
			Name name = node.attribute_value("name", Name());
			Policy * policy = _lookup_policy(name.string());
			if (!policy) {
				Genode::warning("invalid key name \"", name, "\"");
				continue;
			}

			/* if two policies match, give precedence to policy defined first */
			if (policy->defined())
				continue;

			if (!node.has_attribute("label")) {
				Genode::warning("missing 'label' attribute for key ", name);
				continue;
			}

			/* assign policy to matching client session */
			for (Session *s = session_list.first(); s; s = s->next())
				if (node.attribute("label").has_value(s->label().string()))
					policy->client(s);
		}

	} catch (Xml_node::Nonexistent_sub_node) { }
}

