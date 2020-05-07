/*
 * \brief  Tokenizer test
 * \author Norman Feske
 * \date   2020-05-08
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>
#include <base/log.h>
#include <base/component.h>
#include <base/attached_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

using namespace Genode;


/**
 * Regression test for issue #3756
 */
static void test_out_of_bounds_access(Env &env)
{
	enum { PAGE_SIZE   = 4096U,
	       SUB_RM_SIZE = PAGE_SIZE*2,
	       BUF_SIZE    = PAGE_SIZE };

	Rm_connection rm(env);
	Region_map_client sub_rm(rm.create(SUB_RM_SIZE));

	/* allocate physical page of memory as buffer */
	Attached_ram_dataspace buf_ds(env.ram(), env.rm(), BUF_SIZE);

	/* attach buffer at start of managed dataspace, leave 2nd page as guard */
	sub_rm.attach_at(buf_ds.cap(), 0);

	/* locally attach managed dataspace */
	char * const buf_ptr = env.rm().attach(sub_rm.dataspace());

	auto tokenize_two_tokens_at_end_of_buffer = [&] (char const * const input)
	{
		log("tokenize: '", input, "'");

		size_t const input_len = strlen(input);
		char * const token_ptr = buf_ptr + BUF_SIZE - input_len;
		memcpy(token_ptr, input, input_len);

		typedef ::Genode::Token<Scanner_policy_identifier_with_underline> Token;

		Token t(token_ptr, input_len);

		t = t.next();
	};

	tokenize_two_tokens_at_end_of_buffer("x ");
	tokenize_two_tokens_at_end_of_buffer("x\"");
}


void Component::construct(Env &env)
{
	log("--- token test ---");

	test_out_of_bounds_access(env);

	log("--- finished token test ---");
}
