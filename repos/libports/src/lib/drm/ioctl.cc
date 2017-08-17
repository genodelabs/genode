/**
 * \brief  Called for every DRM function (implement me)
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {
#include <drm.h>
#include <i915_drm.h>
}

#include <base/log.h>


extern "C" int genode_ioctl(int fd, unsigned long request, void *arg)
{
	Genode::error(__func__, " not implemented");
	return -1;
}
