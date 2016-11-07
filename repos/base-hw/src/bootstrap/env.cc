/*
 * \brief  Environment dummy implementation needed by cxx library
 * \author Stefan Kalkowski
 * \date   2016-09-23
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* base includes */
#include <base/env.h>

/* core includes */
#include <assert.h>

Genode::Env_deprecated * Genode::env_deprecated()
{
	assert(false);
	return nullptr;
}
