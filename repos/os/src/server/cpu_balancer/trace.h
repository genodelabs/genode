/*
 * \brief  Data from Trace session,
 *         e.g. CPU idle times && thread execution time
 * \author Alexander Boettcher
 * \date   2020-07-22
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_H_
#define _TRACE_H_

#include <util/reconstructible.h>
#include <trace_session/connection.h>

namespace Cpu {
	class Trace;
	class Sleeper;

	using Genode::Constructible;
	using Genode::Trace::Subject_id;
	using Genode::Trace::Subject_info;
	using Genode::Affinity;
	using Genode::Session_label;
	using Genode::Trace::Thread_name;
	using Genode::Trace::Connection;
	using Genode::Trace::Execution_time;
};

class Cpu::Trace
{
	private:

		Genode::Env              &_env;
		Affinity::Space const     _space;
		Constructible<Connection> _trace { };

		Genode::size_t _arg_quota { 12 * 4096 };
		Genode::size_t _ram_quota { _arg_quota + 4 * 4096 };

		enum { MAX_CORES = 64, MAX_THREADS = 2, HISTORY = 4 };

		Execution_time  _idle_times[MAX_CORES][MAX_THREADS][HISTORY];
		Execution_time  _idle_max  [MAX_CORES][MAX_THREADS];
		unsigned        _idle_slot { HISTORY - 1 };

		unsigned        _subject_id_reread { 0 };

		void _reconstruct(Genode::size_t const upgrade = 4 * 4096)
		{
			_ram_quota += upgrade;
			_arg_quota += upgrade;

			_trace.destruct();
			_trace.construct(_env, _ram_quota, _arg_quota);

			/*
			 * Explicitly re-trigger import of subjects. Otherwise
			 * stored trace ids are not valid if used with subject_info(id)
			 * and we get exception thrown about unknown ids.
			 */
			_trace->_retry<Genode::Trace::Session::Alloc_rpc_error>([&] {
				return _trace->call<Genode::Trace::Session::Rpc_subjects>(); });

			_subject_id_reread ++;
		}

		Affinity::Space _sanitize_space(Affinity::Space const space)
		{
			unsigned width  = space.width();
			unsigned height = space.height();

			if (_space.width() > MAX_CORES)
				width = MAX_CORES;
			if (_space.height() > MAX_THREADS)
				height = MAX_THREADS;

			if (width != space.width() || height != space.height())
				Genode::error("supported affinity space too small");

			return Affinity::Space(width, height);
		}

		void _read_idle_times(bool);

	public:

		Trace(Genode::Env &env)
		: _env(env), _space(_sanitize_space(env.cpu().affinity_space()))
		{
			_reconstruct();
			_read_idle_times(true);
		}

		void read_idle_times() { _read_idle_times(false); }

		unsigned subject_id_reread() const { return _subject_id_reread; }
		void subject_id_reread_reset()     { _subject_id_reread = 0; }

		Execution_time read_max_idle(Affinity::Location const &location)
		{
			unsigned const xpos = location.xpos();
			unsigned const ypos = location.ypos();

			if (xpos >= MAX_CORES || ypos >= MAX_THREADS)
				return Execution_time(0, 0);

			return _idle_max[xpos][ypos];
		}


		Subject_id lookup_missing_id(Session_label const &,
		                             Thread_name const &);

		Session_label lookup_my_label();

		template <typename FUNC>
		void retrieve(Subject_id const target_id, FUNC const &fn)
		{
			if (!_trace.constructed())
				return;

			auto count = _trace->for_each_subject_info([&](Subject_id const &id,
			                                               Subject_info const &info) {
				if (id == target_id)
					fn(info.execution_time(), info.affinity());
			});

			if (count.count == count.limit) {
				Genode::log("reconstruct trace session, subject_count=", count.count);
				_reconstruct();
			}
		}

		Execution_time abs_idle_times(Affinity::Location const &location)
		{
			if (location.xpos() >= MAX_CORES || location.ypos() >= MAX_THREADS)
				return Execution_time(0, 0);
			return _idle_times[location.xpos()][location.ypos()][_idle_slot];
		}

		Execution_time diff_idle_times(Affinity::Location const &location)
		{
			unsigned const xpos = location.xpos();
			unsigned const ypos = location.ypos();

			if (xpos >= MAX_CORES || ypos >= MAX_THREADS)
				return Execution_time(0, 0);

			Execution_time const &prev = _idle_times[xpos][ypos][((_idle_slot == 0) ? unsigned(HISTORY) : _idle_slot) - 1];
			Execution_time const &curr = _idle_times[location.xpos()][location.ypos()][_idle_slot];

			using Genode::uint64_t;

			uint64_t ec = (prev.thread_context < curr.thread_context) ?
			              curr.thread_context - prev.thread_context : 0;
			uint64_t sc = (prev.scheduling_context < curr.scheduling_context) ?
			              curr.scheduling_context - prev.scheduling_context : 0;

			/* strange case where idle times are not reported if no threads are on CPU */
			if (!ec && !sc && curr.thread_context == 0 && curr.scheduling_context == 0)
				return Execution_time { 1, 1 };

			return Execution_time { ec, sc };
		}
};

struct Cpu::Sleeper : Genode::Thread
{
	Genode::Env                   &_env;
	Genode::Blockade               _block { };

	Sleeper(Genode::Env &env, Location const &location)
	:
		Genode::Thread(env, Name("sleep_", location.xpos(), "x", location.ypos()),
		               2 * 4096, location, Weight(), env.cpu()),
		_env(env)
	{ }

	void entry() override
	{
		while (true) {
			_block.block();
		}
	}
};
#endif /* _TRACE_H_ */
