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

#include <util/list.h>

#include "chunk.h"

struct Lru_policy
{
	class Element : public Genode::List<Element>::Element {};

	static void read(const Element  *e);
	static void write(const Element *e);
	static void flush(Cache::size_t size = 0);
};
