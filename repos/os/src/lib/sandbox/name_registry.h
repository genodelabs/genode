/*
 * \brief  Interface for database of child names
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__NAME_REGISTRY_H_
#define _LIB__SANDBOX__NAME_REGISTRY_H_

/* Genode includes */
#include <base/child.h>

/* local includes */
#include <types.h>

namespace Sandbox { struct Name_registry; }

struct Sandbox::Name_registry
{
	virtual ~Name_registry() { }

	typedef Child_policy::Name Name;

	/**
	 * Return child name for a given alias name
	 *
	 * If there is no alias, the function returns the original name.
	 */
	virtual Name deref_alias(Name const &) = 0;
};

#endif /* _LIB__SANDBOX__NAME_REGISTRY_H_ */
