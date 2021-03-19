/**
 * \brief  Genode C bindings for EGL back end
 * \author Sebastian Sumpf
 * \date   2017-08-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* needed since there exists a 'util/list.h' in mesa */
#include <../include/util/list.h>
#include <blit/blit.h>
#include <base/log.h>

extern "C" {
#include <platform.h>
}

void genode_blit(void const *src, unsigned src_w, void *dst, unsigned dst_w, int w, int h)
{
	blit(src, src_w, dst, dst_w, w, h);
}
