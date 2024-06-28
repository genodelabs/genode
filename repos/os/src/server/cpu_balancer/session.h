/*
 * \brief  CPU session definition
 * \author Alexander Boettcher
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_H_
#define _SESSION_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/registry.h>
#include <base/rpc_server.h>
#include <base/trace/types.h>
#include <cpu_session/client.h>
#include <os/reporter.h>

#include "policy.h"

namespace Cpu {
	using namespace Genode;

	using Genode::Trace::Subject_id;

	class Session;
	class Trace;
	class Policy;
	struct Thread_client;
	using Client_id     = Id_space<Parent::Client>::Element;
	using Child_list    = Registry<Registered<Session> >;
	using Thread_list   = Registry<Registered<Thread_client> >;
	using Ram_allocator = Constrained_ram_allocator;
}

struct Cpu::Thread_client : Interface
{
		Thread_capability      _cap    { };
		Genode::Thread::Name   _name   { };
		Subject_id             _id     { };

		enum Policy_type { NONE, PIN, ROUND_ROBIN, MAX_UTIL };

		Policy_type            _type { Policy_type::NONE };

		Policy_pin             _policy_pin  { };
		Policy_round_robin     _policy_rr   { };
		Policy_max_utilize     _policy_max  { };
		Policy_none            _policy_none { };

		bool                   _fix    { false };

		Cpu::Policy & active_policy() {
			switch (_type) {
			case Policy_type::PIN:
				return _policy_pin;
			case Policy_type::ROUND_ROBIN:
				return _policy_rr;
			case Policy_type::MAX_UTIL:
				return _policy_max;
			case Policy_type::NONE:
				return _policy_none;
			}
			return _policy_none;
		}
};

class Cpu::Session : public Rpc_object<Cpu_session>
{
	private:

		Child_list             &_list;
		Env                    &_env;
		Attached_rom_dataspace &_config;

		Ram_quota_guard        _ram_guard;
		Cap_quota_guard        _cap_guard;
		Ram_allocator          _ram      { _env.pd(), _ram_guard, _cap_guard };
		Heap                   _md_alloc { _ram, _env.rm() };

		Ram_quota              _reclaim_ram { 0 };
		Cap_quota              _reclaim_cap { 0 };

		Parent::Client         _parent_client { };
		Client_id const        _id            { _parent_client,
		                                        _env.id_space() };
		Cpu_session_client     _parent;
		Cpu::Policy::Name      _default_policy { "none" };

		Session::Label   const _label;
		Affinity         const _affinity;

		Thread_list            _threads { };

		bool                   _report  { true  };
		bool                   _verbose;
		bool                   _by_us   { false };

		void switch_policy(Thread::Name const &name, Thread_client &thread,
		                   Affinity::Location const loc)
		{
			if (name == "pin")
				thread._type = Thread_client::Policy_type::PIN;
			else if (name == "round-robin")
				thread._type = Thread_client::Policy_type::ROUND_ROBIN;
			else if (name == "max-utilize")
				thread._type = Thread_client::Policy_type::MAX_UTIL;
			else
				thread._type = Thread_client::Policy_type::NONE;

			thread.active_policy().location = loc;
		}

		bool _one_valid_thread() const
		{
			bool valid = false;

			_threads.for_each([&](auto &thread) {
				if (valid)
					return;

				if (thread._cap.valid())
					valid = true;
			});

			return valid;
		}

		template <typename FUNC>
		void _for_each_thread(FUNC const &fn)
		{
			bool done = false;

			_threads.for_each([&](auto &thread) {
				if (done)
					return;

				done = fn(thread);
			});
		}

		template <typename FUNC>
		void _for_each_thread(FUNC const &fn) const
		{
			bool done = false;

			_threads.for_each([&](auto const &thread) {
				if (done)
					return;

				done = fn(thread);
			});
		}

		template <typename FUNC>
		void kill(Thread_capability const &cap, FUNC const &fn)
		{
			_for_each_thread([&](Thread_client &thread) {
				if (!(thread._cap.valid()) || !(thread._cap == cap))
					return false;

				fn(thread._cap, thread._name, thread._id, thread.active_policy());

				destroy(_md_alloc, &thread);

				return true;
			});
		}

		template <typename FUNC>
		void apply(FUNC const &fn)
		{
			_for_each_thread([&](Thread_client &thread) {
				if (!thread._cap.valid())
					return false;

				return fn(thread._cap, thread._name, thread._id,
				          thread.active_policy(), thread._fix);
			});
		}

		template <typename FUNC>
		void lookup(Thread::Name const &name, FUNC const &fn)
		{
			if (!name.valid())
				return;

			_for_each_thread([&](Thread_client &thread) {
				if (thread._name != name)
					return false;

				return fn(thread._cap, thread.active_policy());
			});
		}

		template <typename FUNC>
		bool reconstruct_if_active(Cpu::Policy::Name const &policy_name,
		                           Thread::Name const &thread_name,
		                           FUNC const &fn)
		{
			if (!thread_name.valid())
				return false;

			bool done = false;

			_for_each_thread([&](Thread_client &thread) {
				if (thread._name != thread_name)
					return false;

				if (thread._fix) {
					done = true;
					return true;
				}

				if (!thread.active_policy().same_type(policy_name))
					switch_policy(policy_name, thread, thread.active_policy().location);

				fn(thread._cap, thread.active_policy());
				done = true;
				return true;
			});

			return done;
		}

		template <typename FUNC>
		void reconstruct(Cpu::Policy::Name const &policy_name,
		                 Thread::Name const &thread_name,
		                 FUNC const &fn)
		{
			if (!thread_name.valid())
				return;

			if (reconstruct_if_active(policy_name, thread_name, fn))
				return;

			construct(policy_name, [&](Thread_capability const &cap,
			                           Thread::Name &store_name,
			                           Cpu::Policy &policy) {
				store_name = thread_name;
				fn(cap, policy);
			});
		}

		template <typename FUNC>
		void construct(Cpu::Policy::Name const &policy_name, FUNC const &fn)
		{
			Thread_client * thread = nullptr;

			try {
				try {
					thread = new (_md_alloc) Registered<Thread_client>(_threads);

					switch_policy(policy_name, *thread, Affinity::Location());

					fn(thread->_cap, thread->_name, thread->active_policy());

					/* XXX - heuristic */
					thread->_fix = (thread->_name == _label.last_element()) ||
					               (thread->_name == "ep") ||
					               (thread->_name == "signal_proxy") ||
					               (thread->_name == "root");

					if (thread->_fix)
						switch_policy("none", *thread, Affinity::Location());
				} catch (Out_of_ram) { _by_us = true; throw;
				} catch (Out_of_caps) { _by_us = true; throw;
				} catch (Insufficient_ram_quota) { _by_us = true; throw;
				} catch (Insufficient_cap_quota) { _by_us = true; throw; }
			} catch (...) {
				if (thread)
					destroy(_md_alloc, thread);
				throw;
			}
		}

		char const * _withdraw_quota(char const * args)
		{
			/*
			 * Sandbox library can't handle the case of insufficient ram/cap
			 * exception during session creation nor during first
			 * create_thread RPC XXX
			 */

			static char argbuf[Parent::Session_args::MAX_SIZE];
			copy_cstring(argbuf, args, sizeof(argbuf));

			size_t extra_ram = (_ram_guard.avail().value < 24 * 1024) ? 24 * 1024 - _ram_guard.avail().value : 0;

			Ram_quota ram { _ram_guard.avail().value + extra_ram };
			Cap_quota cap { _cap_guard.avail().value };

			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", ram.value);
			Arg_string::set_arg(argbuf, sizeof(argbuf), "cap_quota", cap.value);

/*
			_ram_guard.withdraw(ram);
			_cap_guard.withdraw(cap);
*/

			_reclaim_ram.value += ram.value;
			_reclaim_cap.value += cap.value;

			return argbuf;
		}

		/*
		 * Noncopyable
		 */
		Session(Session const &);
		Session &operator = (Session const &);

	public:

		Session(Env &, Affinity const &, char const *, Child_list &,
		        Attached_rom_dataspace &, bool);
		~Session();

		/***************************
		 ** CPU session interface **
		 ***************************/

		Create_thread_result create_thread(Pd_session_capability,
		                                   Thread::Name const &,
		                                   Affinity::Location, Weight,
		                                   addr_t) override;
		void kill_thread(Thread_capability) override;
		void exception_sigh(Signal_context_capability) override;
		Affinity::Space affinity_space() const override;
		Dataspace_capability trace_control() override;
		int ref_account(Cpu_session_capability) override;
		int transfer_quota(Cpu_session_capability, size_t) override;
		Quota quota() override;
		Capability<Cpu_session::Native_cpu> native_cpu() override;

		/************************
		 ** internal interface **
		 ************************/
		bool match(Label const &label) const { return _label == label; };

		void update(Thread::Name const &, Cpu::Policy::Name const &,
		            Affinity::Location const &);
		void update_if_active(Thread::Name const &, Cpu::Policy::Name const &,
		                      Affinity::Location const &);

		void update_threads();
		void update_threads(Trace &, Session_label const &);
		bool report_state(Xml_generator &);
		void reset_report_state() { _report = false; }
		bool report_update() const { return _report; }

		void default_policy(Cpu::Policy::Name const &policy)
		{
			if (policy != _default_policy)
				_report = true;
			_default_policy = policy;
		}

		template <typename FUNC>
		void upgrade(Root::Upgrade_args const &args, FUNC const &fn)
		{
			Ram_quota ram_args = ram_quota_from_args(args.string());
			Cap_quota cap_args = cap_quota_from_args(args.string());

			bool recreate_args = false;

			if (_reclaim_ram.value) {
				Ram_quota remove { min(_reclaim_ram.value, ram_args.value) };

				_reclaim_ram.value -= remove.value;
				ram_args.value     -= remove.value;
				recreate_args       = true;

				if (remove.value > _ram_guard.avail().value)
					_ram_guard.upgrade(Ram_quota{remove.value - _ram_guard.avail().value});
				_ram_guard.withdraw(remove);
			}

			if (_reclaim_cap.value) {
				Cap_quota remove { min(_reclaim_cap.value, cap_args.value) };

				_reclaim_cap.value -= remove.value;
				cap_args.value     -= remove.value;
				recreate_args       = true;

				if (remove.value > _cap_guard.avail().value)
					_cap_guard.upgrade(Cap_quota{remove.value - _cap_guard.avail().value});
				_cap_guard.withdraw(remove);
			}

			_ram_guard.upgrade(ram_args);
			_cap_guard.upgrade(cap_args);

			/* request originated by us */
			if (_by_us) {
				_by_us = false;
				/* due to upgrade ram/cap the next call should succeed */
				return;
			}

			/* track how many resources we donated to parent done by fn() call */
			_ram_guard.withdraw(ram_args);
			_cap_guard.withdraw(cap_args);

			/* rewrite args if we removed some for reclaim quirk */
			if (recreate_args) {
				if (ram_args.value || cap_args.value) {
					char argbuf[Root::Upgrade_args::MAX_SIZE];
					copy_cstring(argbuf, args.string(), sizeof(argbuf));

					Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", ram_args.value);
					Arg_string::set_arg(argbuf, sizeof(argbuf), "cap_quota", cap_args.value);

					fn(_id.id(), argbuf);
				} /* else no upgrade to parent, we consumed all */
			} else
				fn(_id.id(), args);
		}
};

#endif /* _SESSION_H_ */
