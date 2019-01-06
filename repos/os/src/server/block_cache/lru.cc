/*
 * \brief  Least-recently-used cache replacement strategy
 * \author Stefan Kalkowski
 * \date   2013-12-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "lru.h"
#include "driver.h"

typedef Driver<Lru_policy>::Chunk_level_4 Chunk;

static const Lru_policy::Element        *lru = 0;
static Genode::List<Lru_policy::Element> lru_list;


static void lru_access(const Lru_policy::Element *e)
{
	if (e == lru) return;

	if (e->next()) lru_list.remove(e);

	lru_list.insert(e, lru);
	lru = e;
}


void Lru_policy::read(const Lru_policy::Element  *e) {
	lru_access(e); }


void Lru_policy::write(const Lru_policy::Element *e) {
	lru_access(e); }


void Lru_policy::flush(Cache::size_t size)
{
	Cache::size_t s = 0;
	for (Lru_policy::Element *e = lru_list.first();
		 e && ((size == 0) || (s < size));
		 e = lru_list.first(), s += sizeof(Chunk)) {
		Chunk *cb = static_cast<Chunk*>(e);
		e = e->next();
		try {
			cb->free(Driver<Lru_policy>::CACHE_BLK_SIZE,
			         cb->base_offset());
			lru_list.remove(cb);
		} catch(Chunk::Dirty_chunk &e) {
			cb->sync(e.size, e.off);
		}
	}

	if (!lru_list.first()) lru = 0;

	if (s < size) throw Block::Driver::Request_congestion();
}
