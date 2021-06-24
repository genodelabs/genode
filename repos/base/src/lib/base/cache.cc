/*
 * \brief  Implementation of the cache operations
 * \author Christian Prochaska
 * \date   2014-05-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <cpu/cache.h>

/*
 * These functions need to be implemented only for base platforms with ARM
 * support right now, so the default implementation does nothing.
 */

void Genode::cache_coherent(Genode::addr_t, Genode::size_t) { }


void Genode::cache_clean_invalidate_data(Genode::addr_t, Genode::size_t) { }


void Genode::cache_invalidate_data(Genode::addr_t, Genode::size_t) { }
