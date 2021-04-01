/*
 * \brief  Representation of an alias for a child
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__ALIAS_H_
#define _LIB__SANDBOX__ALIAS_H_

/* local includes */
#include <types.h>

namespace Sandbox { struct Alias; }

struct Sandbox::Alias : List<Alias>::Element, Noncopyable
{
	typedef Child_policy::Name Name;
	typedef Child_policy::Name Child;

	Name const name;

	Child child { }; /* defined by 'update' */

	Alias(Name const &name) : name(name) { }

	class Child_attribute_missing : Exception { };

	/*
	 * \throw Child_attribute_missing
	 */
	void update(Xml_node const &alias)
	{
		if (!alias.has_attribute("child"))
			warning("alias node \"", name, "\" lacks child attribute");

		child = alias.attribute_value("child", Child());
	}
};

#endif /* _LIB__SANDBOX__ALIAS_H_ */
