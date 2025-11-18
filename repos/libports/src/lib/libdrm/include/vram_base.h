/*
 * \brief  Base Vram definition for inheritance
 * \author Sebastian Sumpf
 * \date   2025-11-18
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VRAM_BASE_H_
#define _VRAM_BASE_H_

#include <gpu_session/gpu_session.h>

struct Gpu::Vram
{
	virtual ~Vram() { }
};

#endif /* _VRAM_BASE_H_ */
