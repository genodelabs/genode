/*
 * \brief  Argument type for denoting a network interface
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__NIC_TARGET_H_
#define _MODEL__NIC_TARGET_H_

#include "types.h"

namespace Sculpt { struct Nic_target; }


struct Sculpt::Nic_target : Noncopyable
{
	enum Policy { MANAGED, MANUAL } policy { MANAGED };

	bool manual()  const { return policy == MANUAL; }
	bool managed() const { return policy == MANAGED; }

	/*
	 * The 'UNDEFINED' state is used solely at the startup when neither a
	 * managed or manual policy is known. Once a manually managed 'nic_router'
	 * config is provided, it takes precedence over the 'UNDEFINED' managed
	 * state.
	 */
	enum Type { UNDEFINED, OFF, LOCAL, WIRED, WIFI };

	/**
	 * Interactive selection by the user, used when managed policy is in effect
	 */
	Type managed_type { UNDEFINED };

	/**
	 * Selection by the manually-provided NIC-router configuration
	 */
	Type manual_type { UNDEFINED };

	/**
	  * Constructor
	  */
	Nic_target() { }

	/**
	 * Return currently active NIC target type
	 *
	 * This method never returns 'UNDEFINED'.
	 */
	Type type() const
	{
		/*
		 * Enforce the user's interactive choice to disable networking
		 * even if the NIC target is manually managed.
		 */
		if (managed_type == OFF)
			return OFF;

		Type const result = manual() ? manual_type : managed_type;

		return (result == UNDEFINED) ? OFF : result;
	}

	bool local() const { return type() == LOCAL; }
	bool wired() const { return type() == WIRED; }
	bool wifi()  const { return type() == WIFI; }

	bool nic_router_needed() const { return type() != OFF; }

	bool ready() const { return type() == WIRED ||
	                            type() == WIFI  ||
	                            type() == LOCAL; }
};

#endif /* _MODEL__NIC_TARGET_H_ */
