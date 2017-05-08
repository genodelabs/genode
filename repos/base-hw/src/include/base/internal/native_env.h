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

/* Genode includes */
#include <base/stdint.h>
#include <base/quota_guard.h>

namespace Genode
{
	/**
	 * Upgrade quota of the PD session within my Genode environment non-blocking
	 *
	 * This function doesn't lock the environment when upgrading. This is
	 * needed when doing upgrades in situations where the environment is
	 * already locked due to the operation that triggered the upgrade.
	 */
	void upgrade_pd_quota_non_blocking(Ram_quota, Cap_quota);
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_ */
