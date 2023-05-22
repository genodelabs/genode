/*
 * \brief  Calculate SHA256 hash over data blocks of a size of 4096 bytes
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <util/string.h>

/* libcrypto */
#include <openssl/sha.h>

/* tresor includes */
#include <tresor/sha256_4k_hash.h>

using namespace Genode;
using namespace Tresor;


bool Tresor::check_sha256_4k_hash(void const *data_ptr,
                                  void const *exp_hash_ptr)
{
	uint8_t got_hash[32];
	calc_sha256_4k_hash(data_ptr, &got_hash);
	return !memcmp(&got_hash, exp_hash_ptr, sizeof(got_hash));
}


void Tresor::calc_sha256_4k_hash(void const * const data_ptr,
                                 void       * const hash_ptr)
{
	SHA256_CTX context { };
	if (!SHA256_Init(&context)) {
		class Calc_sha256_4k_hash_init_error { };
		throw Calc_sha256_4k_hash_init_error { };
	}
	if (!SHA256_Update(&context, data_ptr, 4096)) {
		class Calc_sha256_4k_hash_update_error { };
		throw Calc_sha256_4k_hash_update_error { };
	}
	if (!SHA256_Final(static_cast<unsigned char *>(hash_ptr), &context)) {
		class Calc_sha256_4k_hash_final_error { };
		throw Calc_sha256_4k_hash_final_error { };
	}
}
