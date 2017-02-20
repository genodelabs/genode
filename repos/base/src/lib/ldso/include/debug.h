/**
 * \brief  Debugger support
 * \author Sebastian Sumpf
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DEBUG_H_
#define _INCLUDE__DEBUG_H_

#include <util/string.h>
#include <base/log.h>
#include <elf.h>

/* local includes */
#include <types.h>

constexpr bool verbose_link_map  = false;
constexpr bool verbose_lookup    = false;
constexpr bool verbose_exception = false;
constexpr bool verbose_shared    = false;
constexpr bool verbose_loading   = false;

namespace Linker {
	struct Debug;
	struct Link_map;

	struct Object;
	void dump_link_map(Object const &);
}

/*
 * GDB can set a breakpoint at this function to find out when ldso has loaded
 * the binary into memory.
 */
void binary_ready_hook_for_gdb();

/**
 * LIBC debug support
 */
extern "C" void brk(Linker::Debug *, Linker::Link_map *);

struct Linker::Debug
{
	/*
	 * This state value describes the mapping change taking place when
	 * the brk address is called.
	 */
	enum State {
		CONSISTENT, /* mapping change is complete */
		ADD,        /* beginning to add a new object */
		DELETE      /* beginning to remove an object mapping */
	};

	Debug() : Brk(brk) { }

	int             version = 1;       /* unused */
	struct Link_map *map    = nullptr; /* start of link map */

	/*
	 * This is the address of a function internal to the run-time linker, that
	 * will always be called when the linker begins to map in a library or unmap
	 * it, and again when the mapping change is complete.  The debugger can set a
	 * breakpoint at this address if it wants to notice shared object mapping
	 * changes.
	 */
	void  (*Brk)(Debug *, Link_map *);
	State state = CONSISTENT;

	static void state_change(State s, Link_map *m)
	{
		d()->state = s;
		d()->Brk(d(), m);
	}

	static Debug *d()
	{
		static Debug _d;
		return &_d;
	}
};


/**
 * Link map
 */
struct Linker::Link_map
{
	Elf::Addr   addr;    /* base address of library */
	char const *path;    /* path */
	void const *dynamic; /* DYNAMIC section */

	Link_map   *next = nullptr;
	Link_map   *prev = nullptr;

	static Link_map *first;

	static void add(Link_map *map)
	{
		map->next = nullptr;
		if (!first) {
			first           = map;
			Debug::d()->map = map;
			return;
		}

		Link_map *m;
		for (m = first; m->next; m = m->next) ;

		m->next   = map;
		map->prev = m;
	}

	static void remove(Link_map *map)
	{
			if (map->prev)
				map->prev->next = map->next;

			if (map->next)
				map->next->prev = map->prev;

			if (map == first)
				first = map->next;
	}

	static void dump()
	{
		if (!verbose_link_map)
			return;

		for (Link_map *m = first; m; m = m->next)
			log("MAP: addr: ", Hex(m->addr),
			    " dynamic: ", m->dynamic, " ", Cstring(m->path),
			    " m: ", m, " p: ", m->prev, " n: ", m->next);
	}
};

#endif /* _INCLUDE__DEBUG_H_ */
