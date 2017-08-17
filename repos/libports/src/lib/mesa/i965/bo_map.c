/**
 * \brief  DRM bindings
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <drivers/dri/i965/intel_image.h>

void *genode_map_image(__DRIimage *image)
{
	/* map read only */
	drm_intel_bo_map(image->bo, false);
	return image->bo->virtual;
}


void genode_unmap_image(__DRIimage *image)
{
	drm_intel_bo_unmap(image->bo);
}
