/*
 * \brief  Implementation of non-core PD session upgrade
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/native_env.h>

void Genode::upgrade_pd_quota_non_blocking(Ram_quota ram, Cap_quota caps)
{
	internal_env().parent().upgrade(Parent::Env::pd(),
	                                String<100>("ram_quota=", ram, ", "
	                                            "cap_quota=", caps).string());
}
