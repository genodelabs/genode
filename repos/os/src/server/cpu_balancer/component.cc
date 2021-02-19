/*
 * \brief  CPU service proxy that migrates threads depending on policies
 * \author Alexander Boettcher
 * \date   2020-07-16
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/registry.h>
#include <base/signal.h>

#include <cpu_session/cpu_session.h>
#include <timer_session/connection.h>
#include <pd_session/connection.h>

#include "session.h"
#include "config.h"
#include "trace.h"

namespace Cpu {
	struct Balancer;
	struct Pd_root;
	struct Cpu_pd_session;

	using Genode::Affinity;
	using Genode::Attached_rom_dataspace;
	using Genode::Constructible;
	using Genode::Cpu_session;
	using Genode::Insufficient_ram_quota;
	using Genode::Ram_quota;
	using Genode::Rpc_object;
	using Genode::Session_capability;
	using Genode::Signal_handler;
	using Genode::Sliced_heap;
	using Genode::Typed_root;
	using Genode::Pd_session;
}

template <typename EXC, typename T, typename FUNC, typename HANDLER>
auto retry(T &env, FUNC func, HANDLER handler,
           unsigned attempts = ~0U) -> decltype(func())
{
	try {
		for (unsigned i = 0; attempts == ~0U || i < attempts; i++)
			try { return func(); }
			catch (EXC) {
				if ((i + 1) % 5 == 0 || env.pd().avail_ram().value < 8192)
					Genode::warning(i, ". attempt to extend dialog report "
					                "size, ram_avail=", env.pd().avail_ram());

				if (env.pd().avail_ram().value < 8192)
					throw;

				handler();
			}

		throw EXC();
	} catch (Genode::Xml_generator::Buffer_exceeded) {
		Genode::error("not enough memory for xml");
	}
	return;
}

typedef Genode::Registry<Genode::Registered<Cpu::Sleeper> >   Sleeper_list;
typedef Genode::Tslab<Genode::Registered<Cpu::Sleeper>, 4096> Tslab_sleeper;

namespace Cpu {

	template <typename FUNC>
	Session_capability withdraw_quota(Sliced_heap &slice,
	                                  Root::Session_args const &args,
	                                  FUNC const &fn)
	{
		/*
		 * We need to decrease 'ram_quota' by
		 * the size of the session object.
		 */
		Ram_quota const ram_quota = ram_quota_from_args(args.string());

		size_t needed = sizeof(Session) + slice.overhead(sizeof(Session));

		if (needed > ram_quota.value)
			throw Insufficient_ram_quota();

		Ram_quota const remaining_ram_quota { ram_quota.value - needed };

		/*
		 * Validate that the client provided the amount of caps as mandated
		 * for the session interface.
		 */
		Cap_quota const cap_quota = cap_quota_from_args(args.string());

		if (cap_quota.value < Session::CAP_QUOTA)
			throw Insufficient_cap_quota();

		/*
		 * Account for the dataspace capability needed for allocating the
		 * session object from the sliced heap.
		 */
		if (cap_quota.value < 1)
			throw Insufficient_cap_quota();

		Cap_quota const remaining_cap_quota { cap_quota.value - 1 };

		/*
		 * Deduce ram quota needed for allocating the session object from the
		 * donated ram quota.
		 */
		char argbuf[Parent::Session_args::MAX_SIZE];
		copy_cstring(argbuf, args.string(), sizeof(argbuf));

		Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", remaining_ram_quota.value);
		Arg_string::set_arg(argbuf, sizeof(argbuf), "cap_quota", remaining_cap_quota.value);

		return fn(argbuf);
	}
}

struct Cpu::Cpu_pd_session
{
	Parent::Client parent_client { };
	Client_id      id;

	Genode::Capability<Pd_session> pd_cap;

	Cpu_pd_session(Genode::Env              &env,
	               Root::Session_args const &args,
	               Affinity           const &affinity)
	:
		id(parent_client, env.id_space()),
		pd_cap(env.session<Pd_session>(id.id(), args, affinity))
	{ }

	virtual ~Cpu_pd_session() { }
};

struct Cpu::Pd_root : Rpc_object<Typed_root<Pd_session>>
{
	Env                                   &env;
	Sliced_heap                            slice    { env.ram(), env.rm() };
	Registry<Registered<Cpu_pd_session> >  sessions { };

	Session_capability session(Root::Session_args const &args,
	                           Affinity const &affinity) override
	{
		return withdraw_quota(slice, args,
		                      [&] (char const * const session_args) {
			return (new (slice) Registered<Cpu_pd_session>(sessions, env,
			                                               session_args,
			                                               affinity))->pd_cap;
		});
	}

	void upgrade(Session_capability const, Root::Upgrade_args const &) override
	{
		/*
		 * The PD cap (of the parent) is pass through to the client directly,
		 * so we should not get any upgrades here.
		 */
		Genode::warning("Pd upgrade unexpected");
	}

	void close(Session_capability const cap) override
	{
		if (!cap.valid()) { 
			Genode::error("unknown cap");
			return;
		}

		sessions.for_each([&](auto &session) {
			if (session.pd_cap == cap)
				destroy(slice, &session);
		});
	}

	Pd_root(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(*this));
	}
};

struct Cpu::Balancer : Rpc_object<Typed_root<Cpu_session>>
{
	Genode::Env            &env;
	Attached_rom_dataspace  config        { env, "config" };
	Timer::Connection       timer         { env };
	Sliced_heap             slice         { env.ram(), env.rm() };
	Child_list              list          { };
	Constructible<Trace>    trace         { };
	Constructible<Reporter> reporter      { };
	uint64_t                timer_us      { 1000 * 1000UL };
	Session_label           label         { };
	unsigned                report_size   { 4096 * 1 };
	Tslab_sleeper           alloc_thread  { slice };
	Sleeper_list            sleeper       { };
	bool                    verbose       { false };
	bool                    update_report { false };
	bool                    use_sleeper   { true  };

	Cpu::Pd_root            pd            { env };

	Signal_handler<Balancer> signal_config {
		env.ep(), *this, &Balancer::handle_config };

	void handle_config();
	void handle_timeout();

	/*
	 * Need extra EP to avoid dead-lock/live-lock (depending on kernel)
	 * due to down-calls by this component, e.g. parent.upgrade(...), and
	 * up-calls by parent using this CPU service, e.g. to create initial thread
	 *
	 * Additionally, a list_mutex is required due to having 2 EPs now.
	 */
	Entrypoint ep { env, 4 * 4096, "live/dead-lock", Affinity::Location() };

	Signal_handler<Balancer> signal_timeout {
		ep, *this, &Balancer::handle_timeout };

	Genode::Mutex list_mutex { };

	/***********************
	 ** Session interface **
	 ***********************/

	Session_capability session(Root::Session_args const &args,
	                           Affinity const &affinity) override
	{
		return withdraw_quota(slice, args,
		                      [&] (char const * const session_args) {
			if (verbose)
				log("new session '", args.string(), "' -> '", session_args, "' ",
				    affinity.space().width(), "x", affinity.space().height(), " ",
				    affinity.location().xpos(), "x", affinity.location().ypos(),
				    " ", affinity.location().width(), "x", affinity.location().height());

			Mutex::Guard guard(list_mutex);

			auto * session = new (slice) Registered<Session>(list, env,
			                                                 affinity,
			                                                 session_args,
			                                                 list, verbose);

			/* check for config of new session */
			Cpu::Config::apply(config.xml(), list);

			return session->cap();
		});
	}

	void upgrade(Session_capability const cap, Root::Upgrade_args const &args) override
	{ 
		if (!args.valid_string()) return;

		auto lambda = [&] (Rpc_object_base *rpc_obj) {
			if (!rpc_obj)
				return;

			Session *session = dynamic_cast<Session *>(rpc_obj);
			if (!session)
				return;

			session->upgrade(args, [&](auto id, auto const &adjusted_args) {
				env.upgrade(id, adjusted_args);
			});
		};

		Mutex::Guard guard(list_mutex);

		env.ep().rpc_ep().apply(cap, lambda);
	}

	void close(Session_capability const cap) override
	{
		if (!cap.valid()) return;

		Mutex::Guard guard(list_mutex);

		Session *object = nullptr;

		env.ep().rpc_ep().apply(cap,
			[&] (Session *source) {
				object = source;
		});

		if (!object)
			return;

		destroy(slice, object);

		update_report = true;
	}

	/*****************
	 ** Constructor **
	 *****************/

	Balancer(Genode::Env &env) : env(env)
	{
		config.sigh(signal_config);
		timer.sigh(signal_timeout);

		Affinity::Space const space = env.cpu().affinity_space();
		Genode::log("affinity space=",
		            space.width(), "x", space.height());

		for (unsigned i = 0; i < space.total(); i++) {
			Affinity::Location location = env.cpu().affinity_space().location_of_index(i);
			Sleeper *t = new (alloc_thread) Genode::Registered<Sleeper>(sleeper, env, location);
			t->start();
		}

		handle_config();

		/* first time start ever timeout */
		timer.trigger_periodic(timer_us);

		env.parent().announce(env.ep().manage(*this));
	}
};

void Cpu::Balancer::handle_config()
{
	config.update();

	bool use_trace   = true;
	bool use_report  = true;
	uint64_t time_us = timer_us;

	if (config.valid()) {
		use_trace   = config.xml().attribute_value("trace", use_trace);
		use_report  = config.xml().attribute_value("report", use_report);
		time_us     = config.xml().attribute_value("interval_us", timer_us);
		verbose     = config.xml().attribute_value("verbose", verbose);
		use_sleeper = config.xml().attribute_value("sleeper", use_sleeper);

		/* read in components configuration */
		Cpu::Config::apply(config.xml(), list);
	}

	if (verbose)
		log("config update - verbose=", verbose, ", trace=", use_trace,
		    ", report=", use_report, ", interval=", timer_us,"us");

	/* also start all subsystem if no valid config is available */
	trace.conditional(use_trace, env);
	if (use_trace && !label.valid())
		label = trace->lookup_my_label();

	reporter.conditional(use_report, env, "components", "components", report_size);
	if (use_report)
		reporter->enabled(true);

	if (timer_us != time_us)
		timer.trigger_periodic(time_us);
}

void Cpu::Balancer::handle_timeout()
{
	Mutex::Guard guard(list_mutex);

	if (use_sleeper) {
		/* wake all sleepers to get more accurate idle CPU utilization times */
		sleeper.for_each([](auto &thread) {
			thread._block.wakeup(); });
	}

	/* remember current reread state */
	unsigned reread_subjects = 0;
	if (trace.constructed()) {
		reread_subjects = trace->subject_id_reread();
		trace->read_idle_times();
	}

	/* update all sessions */
	list.for_each([&](auto &session) {
		if (trace.constructed()) {
			session.update_threads(*trace, label);
		}
		else
			session.update_threads();

		if (session.report_update())
			update_report = true;
	});

	/* reset reread state if it did not change in between */
	if (trace.constructed() && trace->subject_id_reread() &&
	    reread_subjects == trace->subject_id_reread())
			trace->subject_id_reread_reset();

	if (reporter.constructed() && update_report) {
		bool reset_report = false;

		retry<Genode::Xml_generator::Buffer_exceeded>(env, [&] () {
			Reporter::Xml_generator xml(*reporter, [&] () {
				list.for_each([&](auto &session) {
					reset_report |= session.report_state(xml);
				});
			});
		}, [&] () {
			report_size += 4096;
			reporter.construct(env, "components", "components", report_size);
			reporter->enabled(true);
		});

		if (reset_report) {
			list.for_each([](auto &session) {
				session.reset_report_state();
			});
		}

		update_report = false;
	}
}

void Component::construct(Genode::Env &env)
{
	static Cpu::Balancer server(env);
}
