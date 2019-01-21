/*
 * \brief  Test for dynamic resource requests
 * \author Norman Feske
 * \date   2013-09-27
 *
 * This test exercises various situations where a component might need to
 * request additional resources from its parent.
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <pd_session/connection.h>
#include <os/reporter.h>

namespace Test {
	using namespace Genode;
	struct Monitor;
}


static void print_quota_stats(Genode::Pd_session &pd)
{
	Genode::log("quota: avail=", pd.avail_ram().value, " used=", pd.used_ram().value);
}


#define ASSERT(cond) \
	if (!(cond)) { \
		error("assertion ", #cond, " failed"); \
		throw Error(); }


struct Test::Monitor
{
	Env &_env;

	Attached_rom_dataspace _init_state { _env, "state" };

	Reporter _init_config { _env, "init.config" };

	size_t _ram_quota = 2*1024*1024;

	void _gen_service_xml(Xml_generator &xml, char const *name)
	{
		xml.node("service", [&] () { xml.attribute("name", name); });
	};

	void _generate_init_config()
	{
		Reporter::Xml_generator xml(_init_config, [&] () {

			xml.node("report", [&] () { xml.attribute("child_ram", true); });

			xml.node("parent-provides", [&] () {
				_gen_service_xml(xml, "ROM");
				_gen_service_xml(xml, "CPU");
				_gen_service_xml(xml, "PD");
				_gen_service_xml(xml, "LOG");
				_gen_service_xml(xml, "Timer");
			});

			xml.node("start", [&] () {
				xml.attribute("name", "test-resource_request");
				xml.attribute("caps", 3000);
				xml.node("resource", [&] () {
					xml.attribute("name", "RAM");
					xml.attribute("quantum", _ram_quota);
				});
				xml.node("route", [&] () {
					xml.node("any-service", [&] () {
						xml.node("parent", [&] () { }); }); });
			});
		});
	}

	size_t _resource_request_from_init_state()
	{
		try {
			return _init_state.xml().sub_node("child")
			                        .sub_node("ram")
			                        .attribute_value("requested", Number_of_bytes(0));
		}
		catch (...) { return 0; }
	}

	Signal_handler<Monitor> _init_state_handler {
		_env.ep(), *this, &Monitor::_handle_init_state };

	void _handle_init_state()
	{
		_init_state.update();

		size_t const requested = _resource_request_from_init_state();
		if (requested > 0) {
			log("responding to resource request of ", Number_of_bytes(requested));

			_ram_quota += requested;
			_generate_init_config();
		}
	}

	Monitor(Env &env) : _env(env)
	{
		_init_config.enabled(true);
		_init_state.sigh(_init_state_handler);
		_generate_init_config();
	}
};


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	/*
	 * Distinguish the roles of the program. If configured as playing the
	 * monitor role, it manages the configuration of a sub init and monitors
	 * the init state for resource requests.
	 */
	Attached_rom_dataspace config { env, "config" };
	if (config.xml().attribute_value("role", String<32>()) == "monitor") {
		static Test::Monitor monitor(env);
		return;
	}

	class Error : Exception { };

	log("--- test-resource_request started ---");

	/*
	 * Consume initial quota to let the test trigger the corner cases of
	 * exceeded quota.
	 */
	size_t const avail_quota = env.pd().avail_ram().value;
	enum { KEEP_QUOTA = 64*1024 };
	size_t const wasted_quota = (avail_quota >= KEEP_QUOTA)
	                          ?  avail_quota -  KEEP_QUOTA : 0;
	if (wasted_quota)
		env.ram().alloc(wasted_quota);

	log("wasted available quota of ", wasted_quota, " bytes");

	print_quota_stats(env.pd());

	/*
	 * Drain PD session by allocating a lot of signal-context capabilities.
	 * This step will ultimately trigger resource requests to the parent.
	 */
	log("\n-- draining PD session --");
	{
		struct Dummy_signal_handler : Signal_handler<Dummy_signal_handler>
		{
			Dummy_signal_handler(Entrypoint &ep)
			: Signal_handler<Dummy_signal_handler>(ep, *this, nullptr) { }
		};
		enum { NUM_SIGH = 1000U };
		static Constructible<Dummy_signal_handler> dummy_handlers[NUM_SIGH];

		for (unsigned i = 0; i < NUM_SIGH; i++)
			dummy_handlers[i].construct(env.ep());

		print_quota_stats(env.pd());

		for (unsigned i = 0; i < NUM_SIGH; i++)
			dummy_handlers[i].destruct();
	}
	print_quota_stats(env.pd());
	size_t const used_quota_after_draining_session = env.pd().used_ram().value;

	/*
	 * When creating a new session, we try to donate RAM quota to the server.
	 * Because, we don't have any RAM quota left, we need to issue another
	 * resource request to the parent.
	 */
	log("\n-- out-of-memory during session request --");
	static Pd_connection pd(env);
	pd.ref_account(env.pd_session_cap());
	print_quota_stats(env.pd());
	size_t const used_quota_after_session_request = env.pd().used_ram().value;

	/*
	 * Quota transfers from the component's RAM session may result in resource
	 * requests, too.
	 */
	log("\n-- out-of-memory during transfer-quota --");
	env.pd().transfer_quota(pd.cap(), Ram_quota{512*1024});
	print_quota_stats(env.pd());
	size_t const used_quota_after_transfer = env.pd().used_ram().value;

	/*
	 * Finally, resource requests could be caused by a regular allocation,
	 * which is the most likely case in normal scenarios.
	 */
	log("\n-- out-of-memory during RAM allocation --");
	env.pd().alloc(512*1024);
	print_quota_stats(env.pd());
	size_t const used_quota_after_alloc = env.pd().used_ram().value;

	/*
	 * Validate asserted effect of the individual steps on the used quota.
	 */
	ASSERT(used_quota_after_session_request == used_quota_after_draining_session);
	ASSERT(used_quota_after_transfer        == used_quota_after_session_request);
	ASSERT(used_quota_after_alloc           >  used_quota_after_transfer);

	log("--- finished test-resource_request ---");
	env.parent().exit(0);
}
