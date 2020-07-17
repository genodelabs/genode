/*
 * \brief  Config evaluation
 * \author Alexander Boettcher
 * \date   2020-07-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/session_label.h>
#include <base/thread.h>

#include "config.h"

void Cpu::Config::apply(Xml_node const &start, Child_list &sessions)
{
	using Genode::Session_label;
	using Genode::String;

	typedef String<Session_label::capacity()> Label;

	start.for_each_sub_node("component", [&](Xml_node const &node) {
		if (!node.has_attribute("label"))
			return;

		Label const label = node.attribute_value("label", Label(""));

		sessions.for_each([&](auto &session) {
			if (!session.match(label))
				return;

			if (node.has_attribute("default_policy")) {
				Cpu::Policy::Name const policy = node.attribute_value("default_policy", Cpu::Policy::Name());
				session.default_policy(policy);
			}

			node.for_each_sub_node("thread", [&](Xml_node const &thread) {
				if (!thread.has_attribute("name") || !thread.has_attribute("policy"))
					return;

				Thread::Name const name = thread.attribute_value("name", Thread::Name());
				Cpu::Policy::Name const policy = thread.attribute_value("policy", Cpu::Policy::Name());

				/* explicitly create invalid width/height */
				/* used during thread construction in policy static case */
				Affinity::Location location { 0, 0, 0, 0};

				if (thread.has_attribute("xpos") && thread.has_attribute("ypos"))
					location = Affinity::Location(thread.attribute_value("xpos", 0U),
					                              thread.attribute_value("ypos", 0U),
					                              1, 1);

				session.config(name, policy, location);
			});
		});
	});
}
