/*
 * \brief  Native extensions of the Genode environment
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode
{
	/**
	 * Upgrade quota of the PD session within my Genode environment
	 */
	void upgrade_pd_session_quota(Genode::size_t);
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_ENV_H_ */
