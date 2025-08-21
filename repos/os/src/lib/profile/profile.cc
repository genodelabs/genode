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

#include <base/log.h>
#include <base/mutex.h>
#include <util/construct_at.h>

#include <profile/profile.h>

namespace Profile {

	Genode::uint32_t           _ticks_1000_per_ms { 1'000 }; /* 1GHz TSC */
	Genode::List<Thread_info> *_threads           { nullptr };

	template <typename FN>
	void with_thread(Genode::Thread::Name const &name, FN && fn)
	{
		for (Thread_info *t = _threads->first(); t; t = t->next()) {
			if (t->name == name) {
				fn(*t);
				break;
			}
		}
	}

	template <typename FN>
	void with_function(Thread_info &th, Genode::addr_t addr, FN && fn)
	{
		bool found = false;
		for (Function_info *f = th.functions.first(); f; f = f->next()) {
			if (f->addr == addr) {
				fn(*f);
				found = true;
				break;
			}
		}

		if (!found) {
			th.obj_alloc.create(addr).with_result(
				[&] (Obj_alloc::Allocation &a) {
					a.deallocate = false;
					fn(a.obj);
					th.functions.insert(&a.obj);
				},
				[&] (Genode::Alloc_error &) {
					Genode::error(Genode::Thread::myself()->name,
					              ": Unable to allocate function-info object for profiling ",
					              "function entry/exit of ",
					              Genode::Hex(addr), ". Profiling data will be incomplete!");
				}
			);
		}
	}

	void print_thread_info(Thread_info &th);
}


void Profile::Function_info::print()
{
	using namespace Genode;
	uint32_t const ms {  ticks_1000 / _ticks_1000_per_ms };
	uint32_t const us { (ticks_1000 % _ticks_1000_per_ms) / (_ticks_1000_per_ms/1000) };
	log("  ", Hex(addr), " ", exit_count, " calls took ", ms, ".", us, " ms");
}


void Profile::print_thread_info(Thread_info &th)
{
	using namespace Genode;

	/* make sure that printing is mutually exclusive */
	static Mutex mutex { };
	Mutex::Guard guard { mutex };

	Trace::Timestamp const now = Trace::timestamp();

	if (!th.interval_ms.value || now - th.last_print < th.interval_ms.value * _ticks_1000_per_ms * 1000)
		return;

	log("Call stack of '", th.name, "':");
	th.stack.for_each([&] (Call_stack::Entry const &e) {
		e.info->print(); });

	log("Thread '", th.name, "' profile:");
	uint32_t total_ticks_1000 { 0 };
	uint32_t skipped_count { 0 };
	for (Function_info *f = th.functions.first(); f; f = f->next()) {
		if (f->exit_count == 0)
			continue;

		total_ticks_1000 += f->ticks_1000;

		if (f->ticks_1000 >= _ticks_1000_per_ms)
			f->print();
		else
			skipped_count++;

		f->reset();
	}
	log("Total time: ", total_ticks_1000 / _ticks_1000_per_ms, " ms (", (now - th.last_print) / (_ticks_1000_per_ms*1000), " ms)");
	log(skipped_count, " functions omitted because they consumed less than 1 ms.");
	th.last_print = Genode::Trace::timestamp();
}


void Profile::init(Genode::uint64_t ticks_per_ms)
{
	if (ticks_per_ms > 1000)
		_ticks_1000_per_ms = (Genode::uint32_t)(ticks_per_ms / 1000);

	if (!_threads) {
		static Genode::List<Thread_info> threads { };
		_threads = &threads;
	}
}


void Profile::enable_myself()
{
	using namespace Profile;

	if (!_threads) return;

	with_thread(Genode::Thread::myself()->name, [&](Thread_info & th) {
		th.enable();
	});
}


void Profile::disable_myself()
{
	using namespace Profile;

	if (!_threads) return;

	with_thread(Genode::Thread::myself()->name, [&](Thread_info & th) {
		th.disable();
	});
}


void Profile::Thread_info::enable()
{
	if (!_threads) {
		Genode::error("Missing call to Profile::init()");
		return;
	}

	if (state == State::INVALID)
		_threads->insert(this);

	state = State::ENABLED;
}


void Profile::Thread_info::disable()
{
	if (!_threads) {
		Genode::error("Missing call to Profile::init()");
		return;
	}

	if (state == State::ENABLED)
		state = State::DISABLED;
}


Profile::Thread_info::Thread_info(Genode::Thread::Name const &name,
                                  Genode::Allocator          &alloc,
                                  Milliseconds         const  interval)
: name(name), obj_alloc(alloc), interval_ms(interval)
{
	Genode::uint64_t const max_ms = (1ULL << 32) / _ticks_1000_per_ms;
	if (interval.value >= max_ms)
		Genode::error("Profiling interval too large for thread ", name,
		              "; maximum interval is: ", max_ms, "ms");
}


extern "C" void __cyg_profile_func_enter (void *this_fn, void *)
{
	using namespace Profile;

	if (!_threads) return;

	with_thread(Genode::Thread::myself()->name, [&](Thread_info & th) {
		if (th.state != Thread_info::State::ENABLED) return;

		with_function(th, (Genode::addr_t)this_fn, [&] (Function_info & fn) {
			if (th.stack.full()) Genode::error(Genode::Thread::myself()->name,
			                                   ": Reached maximum call depth for profiling.");
			th.stack.push(&fn);
		});
	});
}


extern "C" void __cyg_profile_func_exit (void *this_fn, void *)
{
	using namespace Profile;

	if (!_threads) return;

	with_thread(Genode::Thread::myself()->name, [&](Thread_info & th) {
		if (th.state != Thread_info::State::ENABLED) return;

		th.stack.with_last([&] (Call_stack::Entry &e) {
			if (e.info->addr == (Genode::addr_t)this_fn) {
				Timestamp const time = Genode::Trace::timestamp() - e.timestamp;
				e.info->ticks_1000  += (Genode::uint32_t)((time - e.callee_time) / 1000U);
				e.info->exit_count  += 1;

				th.stack.pop();

				/* remove 'time' from caller */
				th.stack.with_last([&] (Call_stack::Entry &caller) {
					caller.callee_time += time; });
			} else {
				Genode::error(Genode::Thread::myself()->name,
				              ": Function exit ", this_fn," does not match call stack.");
				th.stack.for_each([&] (Call_stack::Entry const &e) {
					e.info->print(); });
			}
		});
		print_thread_info(th);
	});
}
