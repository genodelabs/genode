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

#ifndef _BO_MAP_H_
#define _BO_MAP_H_

void *genode_map_image(__DRIimage *image);
void  genode_unmap_image(__DRIimage *image);

#endif /* _BO_MAP_H_ */
