/*
 * \brief  Native extensions of the Genode environment
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_

namespace Genode
{
	/**
	 * Upgrade quota of the PD session's capability slab allocator
	 */
	void upgrade_capability_slab();
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_ */
