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
#include <ram_session/connection.h>


static void print_quota_stats(Genode::Ram_session &ram)
{
	Genode::log("quota: avail=", ram.avail(), " used=", ram.used());
}


#define ASSERT(cond) \
	if (!(cond)) { \
		error("assertion ", #cond, " failed"); \
		throw Error(); }


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	class Error : Exception { };

	log("--- test-resource_request started ---");

	/*
	 * Consume initial quota to let the test trigger the corner cases of
	 * exceeded quota.
	 */
	size_t const avail_quota = env.ram().avail();
	enum { KEEP_QUOTA = 64*1024 };
	size_t const wasted_quota = (avail_quota >= KEEP_QUOTA)
	                          ?  avail_quota -  KEEP_QUOTA : 0;
	if (wasted_quota)
		env.ram().alloc(wasted_quota);

	log("wasted available quota of ", wasted_quota, " bytes");

	print_quota_stats(env.ram());

	/*
	 * Out of memory while upgrading session quotas.
	 *
	 * This test provokes the signal session to consume more resources than
	 * donated via the initial session creation. Once drained, we need to
	 * successively upgrade the session. At one point, we will run out of our
	 * initial quota. Now, before we can issue another upgrade, we first need
	 * to request additional resources.
	 *
	 * Note that the construction of the signal receiver will consume a part
	 * of the quota we preserved as 'KEEP_QUOTA'.
	 */
	log("\n-- draining signal session --");
	{
		struct Dummy_signal_handler : Signal_handler<Dummy_signal_handler>
		{
			Dummy_signal_handler(Entrypoint &ep)
			: Signal_handler<Dummy_signal_handler>(ep, *this, nullptr) { }
		};
		enum { NUM_SIGH = 2000U };
		static Constructible<Dummy_signal_handler> dummy_handlers[NUM_SIGH];

		for (unsigned i = 0; i < NUM_SIGH; i++)
			dummy_handlers[i].construct(env.ep());

		print_quota_stats(env.ram());

		for (unsigned i = 0; i < NUM_SIGH; i++)
			dummy_handlers[i].destruct();
	}
	print_quota_stats(env.ram());
	size_t const used_quota_after_draining_session = env.ram().used();

	/*
	 * When creating a new session, we try to donate RAM quota to the server.
	 * Because, we don't have any RAM quota left, we need to issue another
	 * resource request to the parent.
	 */
	log("\n-- out-of-memory during session request --");
	static Ram_connection ram(env);
	ram.ref_account(env.ram_session_cap());
	print_quota_stats(env.ram());
	size_t const used_quota_after_session_request = env.ram().used();

	/*
	 * Quota transfers from the component's RAM session may result in resource
	 * requests, too.
	 */
	log("\n-- out-of-memory during transfer-quota --");
	int ret = env.ram().transfer_quota(ram.cap(), 512*1024);
	if (ret != 0) {
		error("transfer quota failed (ret = ", ret, ")");
		throw Error();
	}
	print_quota_stats(env.ram());
	size_t const used_quota_after_transfer = env.ram().used();

	/*
	 * Finally, resource requests could be caused by a regular allocation,
	 * which is the most likely case in normal scenarios.
	 */
	log("\n-- out-of-memory during RAM allocation --");
	env.ram().alloc(512*1024);
	print_quota_stats(env.ram());
	size_t const used_quota_after_alloc = env.ram().used();

	/*
	 * Validate asserted effect of the individual steps on the used quota.
	 */
	ASSERT(used_quota_after_session_request == used_quota_after_draining_session);
	ASSERT(used_quota_after_transfer        == used_quota_after_session_request);
	ASSERT(used_quota_after_alloc           >  used_quota_after_transfer);

	log("--- finished test-resource_request ---");
	env.parent().exit(0);
}
