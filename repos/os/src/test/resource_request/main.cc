/*
 * \brief  Test for dynamic resource requests
 * \author Norman Feske
 * \date   2013-09-27
 *
 * This test exercises various situations where a process might need to request
 * additional resources from its parent.
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/log.h>
#include <ram_session/connection.h>


static Genode::size_t used_quota()
{
	return Genode::env()->ram_session()->used();
}


static void print_quota_stats()
{
	Genode::log("quota: avail=", Genode::env()->ram_session()->avail(), " "
	            "used=", used_quota());
}


#define ASSERT(cond) \
	if (!(cond)) { \
		Genode::error("assertion ", #cond, " failed"); \
		return -2; }


int main(int argc, char **argv)
{
	using namespace Genode;

	log("--- test-resource_request started ---");

	/*
	 * Consume initial quota to let the test trigger the corner cases of
	 * exceeded quota.
	 */
	size_t const avail_quota = env()->ram_session()->avail();
	enum { KEEP_QUOTA = 64*1024 };
	size_t const wasted_quota = (avail_quota >= KEEP_QUOTA)
	                          ?  avail_quota -  KEEP_QUOTA : 0;
	if (wasted_quota)
		env()->ram_session()->alloc(wasted_quota);

	log("wasted available quota of ", wasted_quota, " bytes");

	print_quota_stats();

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
		enum { NUM_SIG_CTX = 2000U };
		static Signal_context  sig_ctx[NUM_SIG_CTX];
		static Signal_receiver sig_rec;

		for (unsigned i = 0; i < NUM_SIG_CTX; i++)
			sig_rec.manage(&sig_ctx[i]);

		print_quota_stats();

		for (unsigned i = 0; i < NUM_SIG_CTX; i++)
			sig_rec.dissolve(&sig_ctx[i]);
	}
	print_quota_stats();
	size_t const used_quota_after_draining_session = used_quota();

	/*
	 * When creating a new session, we try to donate RAM quota to the server.
	 * Because, we don't have any RAM quota left, we need to issue another
	 * resource request to the parent.
	 */
	log("\n-- out-of-memory during session request --");
	static Ram_connection ram;
	ram.ref_account(env()->ram_session_cap());
	print_quota_stats();
	size_t const used_quota_after_session_request = used_quota();

	/*
	 * Quota transfers from the process' RAM session may result in resource
	 * requests, too.
	 */
	log("\n-- out-of-memory during transfer-quota --");
	int ret = env()->ram_session()->transfer_quota(ram.cap(), 512*1024);
	if (ret != 0) {
		error("transfer quota failed (ret = ", ret, ")");
		return -1;
	}
	print_quota_stats();
	size_t const used_quota_after_transfer = used_quota();

	/*
	 * Finally, resource requests could be caused by a regular allocation,
	 * which is the most likely case in normal scenarios.
	 */
	log("\n-- out-of-memory during RAM allocation --");
	env()->ram_session()->alloc(512*1024);
	print_quota_stats();
	size_t used_quota_after_alloc = used_quota();

	/*
	 * Validate asserted effect of the individual steps on the used quota.
	 */
	ASSERT(used_quota_after_session_request == used_quota_after_draining_session);
	ASSERT(used_quota_after_transfer        == used_quota_after_session_request);
	ASSERT(used_quota_after_alloc           >  used_quota_after_transfer);

	log("--- finished test-resource_request ---");

	return 0;
}
