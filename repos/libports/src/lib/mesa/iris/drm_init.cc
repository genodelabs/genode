/**
 * \brief  Initialize DRM libraries session interface
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <../include/util/list.h>
#include <base/env.h>

extern "C" {
#include <platform.h>
}

extern void drm_init();

void genode_drm_init()
{
	drm_init();
}
