/*
 * \brief  Capability slab init for kernels without an expandable cap space
 * \author Norman Feske
 * \date   2023-06-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/globals.h>

void Genode::init_cap_slab(Pd_session &, Parent &) { }
