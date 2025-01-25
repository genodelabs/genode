/*
 * \brief  Blit API
 * \author Norman Feske
 * \date   2025-01-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__BLIT_H_
#define _INCLUDE__SPEC__ARM_64__BLIT_H_

#include <blit/types.h>
#include <blit/internal/neon.h>
#include <blit/internal/slow.h>

namespace Blit {

	static inline void back2front  (auto &&... args) { _b2f<Neon>(args...); }
	static inline void blend_xrgb_a(auto &&... args) { Neon::Blend::xrgb_a(args...); }
}

#endif /* _INCLUDE__SPEC__ARM_64__BLIT_H_ */
