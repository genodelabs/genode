/*
 * \brief  Implementation of non-core PD session upgrade
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/native_env.h>

void Genode::upgrade_pd_quota_non_blocking(size_t quota)
{
	internal_env().parent().upgrade(Parent::Env::pd(),
	                                String<64>("ram_quota=", quota).string());
}
