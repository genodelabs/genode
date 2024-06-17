/*
 * \brief  CPU session implementation
 * \author Alexander Boettcher
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "session.h"
#include "config.h"

using namespace Genode;
using namespace Cpu;

Cpu_session::Create_thread_result
Cpu::Session::create_thread(Pd_session_capability const  pd,
                            Name                  const &name_by_client,
                            Affinity::Location    const  location,
                            Weight                const  weight,
                            addr_t                const  utcb)
{
	Create_thread_result result = Create_thread_error::DENIED;

	Name name = name_by_client;
	if (!name.valid())
		name = Name("nobody");

	/* quirk since init can't handle Out_of_* during first create_thread call */
	if ((_reclaim_ram.value || _reclaim_cap.value) && _one_valid_thread()) {
		if (_reclaim_ram.value)
			return Create_thread_error::OUT_OF_RAM;
		if (_reclaim_cap.value)
			return Create_thread_error::OUT_OF_CAPS;
	}

	/* read in potential existing policy for thread */
	Cpu::Config::apply_for_thread(_config.xml(), *this, name);

	lookup(name, [&](Thread_capability &store_cap,
	                 Cpu::Policy &policy)
	{
		if (store_cap.valid())
			return false;

		result = _parent.create_thread(pd, name, location, weight, utcb);
		return result.convert<bool>(

			[&] (Thread_capability cap) {
				/* policy and name are set beforehand */
				store_cap  = cap;

				/* for static case with valid location, don't overwrite config */
				policy.thread_create(location);

				if (_verbose)
					log("[", _label, "] new thread at ",
					    policy.location.xpos(), "x", policy.location.ypos(),
					    ", policy=", policy, ", name='", name, "'");

				return true;
			},

			[&] (Create_thread_error) {
				/* stop creation attempt by saying done */
				return true;
			}
		);
	});

	if (result.ok()) {
		_report = true;
		return result;
	}

	result = _parent.create_thread(pd, name, location, weight, utcb);
	if (result.failed())
		return result;

	/* unknown thread without any configuration */
	construct(_default_policy, [&](Thread_capability &store_cap,
	                               Name &store_name, Cpu::Policy &policy)
	{
		policy.location = location;

		result.with_result(
			[&] (Thread_capability cap) {
				store_cap = cap;
				store_name = name;

			if (_verbose)
				log("[", _label, "] new thread at ",
				    location.xpos(), "x", location.ypos(),
				    ", no policy defined",
				    ", name='", name, "'");
			},
			[&] (Create_thread_error) { });
	});

	if (result.ok())
		_report = true;

	return result;
}


void Cpu::Session::kill_thread(Thread_capability const thread_cap)
{
	if (!thread_cap.valid())
		return;

	kill(thread_cap, [&](Thread_capability &cap, Thread::Name &name,
	                     Subject_id &, Cpu::Policy &)
	{
		cap  = Thread_capability();
		name = Thread::Name();

		_parent.kill_thread(thread_cap);

		_report = true;
	});
}

void Cpu::Session::exception_sigh(Signal_context_capability const h)
{
	_parent.exception_sigh(h);
}

Affinity::Space Cpu::Session::affinity_space() const
{
	return _parent.affinity_space();
}

Dataspace_capability Cpu::Session::trace_control()
{
	return _parent.trace_control();
}

Cpu::Session::Session(Env &env, Affinity const &affinity, char const * args,
                      Child_list &list, Attached_rom_dataspace &config,
                      bool const verbose)
:
	_list(list),
	_env(env),
	_config(config),
	_ram_guard(ram_quota_from_args(args)),
	_cap_guard(cap_quota_from_args(args)),
	_parent(_env.session<Cpu_session>(_id.id(), _withdraw_quota(args), affinity)),
	_label(session_label_from_args(args)),
	_affinity(affinity.space().total() ? affinity : Affinity(Affinity::Space(1,1), Affinity::Location(0,0,1,1))),
	_verbose(verbose)
{
	try {
		/* warm up the heap, we need space for at least one thread object */
		construct(_default_policy, [&](Thread_capability &, Name &,
		                               Cpu::Policy &) {});

		_threads.for_each([&](Thread_client &thread) {
			destroy(_md_alloc, &thread);
		});

		/* finally, make object available via rpc */
		_env.ep().rpc_ep().manage(this);
	} catch (...) {
		env.close(_id.id());
		throw;
	}
}

Cpu::Session::~Session()
{
	/* _threads don't need to be cleaned up, but cause warnings */

	_env.ep().rpc_ep().dissolve(this);

	_env.close(_id.id());
}

int Cpu::Session::ref_account(Cpu_session_capability const cap)
{
	return _parent.ref_account(cap);
}

int Cpu::Session::transfer_quota(Cpu_session_capability const cap,
                                 size_t const size)
{
	return _parent.transfer_quota(cap, size);
}

Cpu_session::Quota Cpu::Session::quota()
{
	return _parent.quota();
}

Capability<Cpu_session::Native_cpu> Cpu::Session::native_cpu()
{
	return _parent.native_cpu();
}

bool Cpu::Session::report_state(Xml_generator &xml)
{
	xml.node("component", [&] () {
		xml.attribute("xpos",   _affinity.location().xpos());
		xml.attribute("ypos",   _affinity.location().ypos());
		xml.attribute("width",  _affinity.location().width());
		xml.attribute("height", _affinity.location().height());
		xml.attribute("label",  _label);

		xml.attribute("default_policy", _default_policy);

		apply([&](Thread_capability const &,
		          Thread::Name const &name,
		          Subject_id const &, Cpu::Policy const &policy,
		          bool const enforced_policy)
		{
			xml.node("thread", [&] () {
				xml.attribute("xpos", policy.location.xpos());
				xml.attribute("ypos", policy.location.ypos());
				xml.attribute("name", name);
				xml.attribute("policy", policy.string());
				if (enforced_policy)
					xml.attribute("enforced", enforced_policy);
			});
			return false;
		});
	});

	return _report;
}

void Cpu::Session::update(Thread::Name const &thread,
                          Cpu::Policy::Name const &policy_name,
                          Affinity::Location const &relativ)
{
	reconstruct(policy_name, thread, [&](Thread_capability const &,
	                                     Cpu::Policy &policy)
	{
		policy.config(relativ);

		if (_verbose) {
			String<12> const loc { policy.location.xpos(), "x", policy.location.ypos() };
			log("[", _label, "] name='", thread, "' "
			    "update policy to '", policy, "' ", loc);
		}
	});

	_report = true;
}

void Cpu::Session::update_if_active(Thread::Name const &thread,
                                    Cpu::Policy::Name const &policy_name,
                                    Affinity::Location const &location)
{
	/*
	 * Policies for existing threads are updated.
	 */
	bool found = reconstruct_if_active(policy_name, thread,
	                                   [&](Thread_capability const &,
	                                       Cpu::Policy &policy)
	{
		policy.config(location);

		if (_verbose) {
			String<12> const loc { policy.location.xpos(), "x", policy.location.ypos() };
			log("[", _label, "] name='", thread, "' "
			    "update policy to '", policy, "' ", loc);
		}
	});

	if (found)
		_report = true;
}
