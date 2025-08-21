/*
 * \brief  Instrument functions for profiling
 * \author Johannes Schlatow
 * \date   2025-08-20
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PROFILE__PROFILE_H_
#define _INCLUDE__PROFILE__PROFILE_H_

#include <base/thread.h>
#include <base/memory.h>
#include <base/allocator.h>
#include <util/list.h>
#include <trace/timestamp.h>

namespace Profile {

	using Timestamp = Genode::Trace::Timestamp;

	struct Milliseconds
	{
		Genode::uint64_t value;
	};

	/* stores number of function executions and spent time */
	struct Function_info : Genode::List<Function_info>::Element
	{
		Genode::addr_t   addr;
		Genode::uint32_t exit_count  { 0 };
		Genode::uint32_t ticks_1000  { 0 };

		void reset() {
			exit_count = 0;
			ticks_1000 = 0;
		}

		void print();

		Function_info(Genode::addr_t a) : addr(a) { }
	};

	/* per-thread call stack */
	struct Call_stack
	{
		enum { SIZE = 1024 };

		struct Entry {
			Function_info *info;
			Timestamp      timestamp;
			Timestamp      callee_time;
		};

		Entry          entries[SIZE];
		Genode::size_t next { 0 };

		bool full() const { return next >= SIZE; }

		void push(Function_info *f)
		{
			if (next < SIZE)
				entries[next] = { f, Genode::Trace::timestamp(), 0 };
			next++;
		}

		void pop() { if (next) next--; }

		template <typename FN>
		void with_last(FN && fn) {
			if (next && next <= SIZE) fn(entries[next-1]); }

		template <typename FN>
		void for_each(FN && fn) const {
			for (Genode::size_t i=0; i < next && i < SIZE; i++)
				fn(entries[i]);
		}
	};

	using Obj_alloc = Genode::Memory::Constrained_obj_allocator<Function_info>;

	/* Per-thread state */
	struct Thread_info : Genode::List<Thread_info>::Element
	{
		Genode::Thread::Name const  name;

		/* allocator for Function_info */
		Obj_alloc                   obj_alloc;

		/* print interval (0=never) */
	  Milliseconds          const interval_ms;

		Timestamp                   last_print { Genode::Trace::timestamp() };

		Call_stack                  stack { };

		Genode::List<Function_info> functions { };

		enum State { INVALID, ENABLED, DISABLED };
		State state { INVALID };

		void enable();
		void disable();

		Thread_info(Genode::Thread::Name const &name,
		            Genode::Allocator          &alloc,
		            Milliseconds         const  interval);
	};

	void init(Genode::uint64_t ticks_per_ms);
	void enable_myself();
	void disable_myself();
}

#endif /* _INCLUDE__PROFILE__PROFILE_H_ */
