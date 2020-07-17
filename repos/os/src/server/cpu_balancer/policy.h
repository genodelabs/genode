/*
 * \brief  Policy evaluation
 * \author Alexander Boettcher
 * \date   2020-10-08
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POLICY_H_
#define _POLICY_H_

#include <base/affinity.h>
#include <base/output.h>

#include "trace.h"

namespace Cpu
{
	typedef Genode::Affinity::Location Location;
	typedef Genode::Trace::Execution_time Execution_time;
	typedef Genode::Cpu_session::Name Name;

	class Policy;
	class Policy_none;
	class Policy_pin;
	class Policy_round_robin;
	class Policy_max_utilize;
};

class Cpu::Policy {

	protected:

		bool _update(Location const &base, Location &current)
		{
			Location now = Location(base.xpos() + location.xpos(),
			                        base.ypos() + location.ypos());

			if ((now.xpos() == current.xpos()) && (now.ypos() == current.ypos()))
				return false;

			if (current.xpos() < base.xpos() || current.ypos() < base.ypos()) {
				Genode::error("affinity location strange, current below base");
				return false;
			}

			unsigned const xpos = current.xpos() - base.xpos();
			unsigned const ypos = current.ypos() - base.ypos();

			if (xpos >= base.width() || ypos >= base.height()) {
				Genode::error("affinity dimension raised");
				return false;
			}

			location = Location(xpos, ypos);

			return true;
		}

	public:

		typedef Genode::String<16> Name;

		Location location { };

		virtual ~Policy() { }
		virtual void config(Location const &) = 0;
		virtual bool update(Location const &, Location &, Execution_time const &) = 0;
		virtual void thread_create(Location const &) = 0;
		virtual bool migrate(Location const &, Location &, Trace *) = 0;

		virtual void print(Genode::Output &output) const = 0;

		virtual bool same_type(Name const &) const = 0;
		virtual char const * string() const = 0;
};

class Cpu::Policy_none : public Cpu::Policy
{
	public:

		void config(Location const &) override { };
		void thread_create(Location const &loc) override { location = loc; }
		bool migrate(Location const &, Location &, Trace *) override {
			return false; }

		bool update(Location const &, Location &, Execution_time const &) override {
			return false; }

		void print(Genode::Output &output) const override {
			Genode::print(output, "none"); }

		bool same_type(Name const &name) const override {
			return name == "none"; }

		char const * string() const override {
			return "none"; }
};

class Cpu::Policy_pin : public Cpu::Policy
{
	public:

		void config(Location const &rel) override {
			location = rel; };

		void thread_create(Location const &loc) override {
			/* for static case with valid location, don't overwrite config */
			if (location.width() * location.height() == 0)
				location = loc;
		}

		bool migrate(Location const &base, Location &current, Trace *) override
		{
			Location to = Location(base.xpos() + location.xpos(),
			                       base.ypos() + location.ypos());

			if ((to.xpos() == current.xpos()) && (to.ypos() == current.ypos()))
				return false;

			current = to;
			return true;
		}

		bool update(Location const &, Location &, Execution_time const &) override {
			return false; }

		void print(Genode::Output &output) const override {
			Genode::print(output, "pin"); }

		bool same_type(Name const &name) const override {
			return name == "pin"; }

		char const * string() const override {
			return "pin"; }
};

class Cpu::Policy_round_robin : public Cpu::Policy
{
	public:

		void config(Location const &) override { };
		void thread_create(Location const &loc) override { location = loc; }

		bool migrate(Location const &base, Location &out, Trace *) override
		{
			int const xpos = (location.xpos() + 1) % base.width();
			int const step = (xpos <= location.xpos()) ? 1 : 0;
			int const ypos = (location.ypos() + step) % base.height();

			Location rel { xpos, ypos, 1, 1 };

			out = Location { int(base.xpos() + rel.xpos()),
			                     base.ypos() + rel.ypos(), 1, 1 };
			return true;
		}

		bool update(Location const &base, Location &current, Execution_time const &) override {
			return _update(base, current); }

		void print(Genode::Output &output) const override {
			Genode::print(output, "round-robin"); }

		bool same_type(Name const &name) const override {
			return name == "round-robin"; }

		char const * string() const override {
			return "round-robin"; }
};

class Cpu::Policy_max_utilize : public Cpu::Policy
{
	private:

		Execution_time _last { };
		Execution_time _time { };

		bool           _last_valid { false };
		bool           _time_valid { false };

		Execution_time _last_utilization() const
		{
			using Genode::uint64_t;

			uint64_t ec = (_last.thread_context < _time.thread_context) ?
			              _time.thread_context - _last.thread_context : 0;
			uint64_t sc = (_last.scheduling_context < _time.scheduling_context) ?
			              _time.scheduling_context - _last.scheduling_context : 0;
			return Execution_time(ec, sc);
		}

	public:

		void config(Location const &) override { };
		void thread_create(Location const &loc) override { location = loc; }

		bool update(Location const &base, Location &current, Execution_time const &time) override {
			_last       = _time;
			_last_valid = _time_valid;

			_time       = time;
			_time_valid = true;

			return _update(base, current); }

		template <typename T>
		bool _migrate(T const current_idle, T const thread_time,
		              T const remote_idle, Location const &current,
		              Location const &to, T const max_idle)
		{
			/*
			 * T  - thread
			 * It - idle time on CPU T is on
			 * Ir - idle time on remote CPU with was most idle
			 *
			 *     T %  | It % | Ir %  | desired behaviour
			 * ------------------------|-----------------------------------------------------------------
			 * A:  x    | ~0   | z     | no migration - on current CPU is idle time
			 * B:  x    | y    | z     | migrate if z > x -- check whether remote CPU could handle this extra utilization
			 * C:
			 *
			 */
			bool case_a = current_idle > 1000; /* XXX which threshold ? */
			bool case_b = thread_time  > remote_idle;

			if (false)
				Genode::log("at ", current.xpos(), "x", current.ypos(),
				            " idle=", current_idle,
				            " last=", thread_time,
				            ", at ", to.xpos(), "x", to.ypos(),
				            " most_idle=", remote_idle,
				            " (max_idle=", max_idle, ")",
				            " case_b=", case_b);

			if (case_a)
				return false;
			if (case_b)
				return false;

			return true;
		}

		bool migrate(Location const &base, Location &current, Trace * trace) override
		{
			if (!trace || !_last_valid || !_time_valid)
				return false;

			Execution_time most_idle    { 0UL, 0UL };
			Execution_time current_idle { 0UL, 0UL };
			Location       to           { current }; /* in case of no idle info */

			for (unsigned x = base.xpos(); x < base.xpos() + base.width(); x++) {
				for (unsigned y = base.ypos(); y < base.ypos() + base.height(); y++) {

					Location       const loc(x, y);
					Execution_time const idle = trace->diff_idle_times(loc);

					if (idle.scheduling_context) {
						if (idle.scheduling_context > most_idle.scheduling_context) {
							most_idle = idle;
							to  = loc;
						}
					} else {
						if (idle.thread_context > most_idle.thread_context) {
							most_idle = idle;
							to  = loc;
						}
					}

					if ((loc.xpos() == current.xpos()) && (loc.ypos() == current.ypos()))
						current_idle = idle;
				}
			}

			if ((to.xpos() == current.xpos()) && (to.ypos() == current.ypos()))
				return false;

			Execution_time const last_util = _last_utilization();

			/* heuristics to not migrate and better stay on same CPU */
			if (last_util.scheduling_context && !last_util.thread_context) {
				if (!_migrate(current_idle.scheduling_context, last_util.scheduling_context,
				              most_idle.scheduling_context, current, to,
				              trace->read_max_idle(current).scheduling_context))
					return false;
			} else {
				if (!_migrate(current_idle.thread_context, last_util.thread_context,
				              most_idle.thread_context, current, to,
				              trace->read_max_idle(current).thread_context))
					return false;
			}

			current     = to;
			_last_valid = false;
			_time_valid = false;

			return true;
		}

		void print(Genode::Output &output) const override {
			Genode::print(output, "max-utilize"); }

		bool same_type(Name const &name) const override {
			return name == "max-utilize"; }

		char const * string() const override {
			return "max-utilize"; }
};

#endif
