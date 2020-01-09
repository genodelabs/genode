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

struct Sandbox::Alias : List<Alias>::Element
{
	typedef String<128> Name;
	typedef String<128> Child;

	Name  name;
	Child child;

	/**
	 * Exception types
	 */
	class Name_is_missing  : Exception { };
	class Child_is_missing : Exception { };

	/**
	 * Constructor
	 *
	 * \throw Name_is_missing
	 * \throw Child_is_missing
	 */
	Alias(Genode::Xml_node alias)
	:
		name (alias.attribute_value("name",  Name())),
		child(alias.attribute_value("child", Child()))
	{

		if (!name.valid())  throw Name_is_missing();
		if (!child.valid()) throw Child_is_missing();
	}
};

#endif /* _LIB__SANDBOX__ALIAS_H_ */
