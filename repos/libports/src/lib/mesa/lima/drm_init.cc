/**
 * \brief  Initialize DRM libraries session interface
 * \author Josef Soentgen
 * \date   2021-04-30
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <../include/util/list.h>
#include <base/env.h>

#include <libdrm/ioctl_dispatch.h>

extern "C" {
#include <platform.h>
}

void genode_drm_init(void)
{
	drm_init(Libdrm::Driver::LIMA);
}
