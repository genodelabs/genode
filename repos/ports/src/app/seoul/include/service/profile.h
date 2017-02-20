/*
 * \brief  Disable original profiling macros of the NOVA userland
 * \author Norman Feske
 * \date   2011-11-19
 *
 * This header has the sole purpose to shadow the orignal 'service/profile.h'
 * header of the NOVA userland. The original macros rely on special support in
 * the linker script. However, we prefer to stick with Genode's generic linker
 * script.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#define COUNTER_INC(NAME)
#define COUNTER_SET(NAME, VALUE)
