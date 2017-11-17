/*
 * \brief  Genode-specific data structures
 * \author Josef Soentgen
 * \date   2017-11-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SDL_GENODE_INTERNAL_H_
#define _SDL_GENODE_INTERNAL_H_

struct Video
{
	bool resize_pending;
	int width;
	int height;
};

#endif /* _SDL_GENODE_INTERNAL_H_ */
