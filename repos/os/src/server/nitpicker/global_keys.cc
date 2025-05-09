/*
 * \brief  Global keys handling
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <global_keys.h>

using namespace Nitpicker;


Global_keys::Policy *Global_keys::_lookup_policy(char const *key_name)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		if (strcmp(key_name, Input::key_name((Input::Keycode)i)) == 0)
			return &_policies[i];

	return 0;
}


void Global_keys::apply_config(Xml_node const &config, Session_list &session_list)
{
	for (unsigned i = 0; i < NUM_POLICIES; i++)
		_policies[i] = Policy();

	config.for_each_sub_node("global-key", [&] (Xml_node const &node) {

		if (!node.has_attribute("name")) {
			warning("attribute 'name' missing in <global-key> config node");
			return;
		}

		using Name = String<32>;
		Name name = node.attribute_value("name", Name());
		Policy * policy = _lookup_policy(name.string());
		if (!policy) {
			warning("invalid key name \"", name, "\"");
			return;
		}

		/* if two policies match, give precedence to policy defined first */
		if (policy->defined())
			return;

		if (!node.has_attribute("label")) {
			warning("missing 'label' attribute for key ", name);
			return;
		}

		/* assign policy to matching client session */
		for (Gui_session *s = session_list.first(); s; s = s->next())
			if (node.attribute_value("label", String<128>()) == s->label().string())
				policy->client(s);
	});
}

