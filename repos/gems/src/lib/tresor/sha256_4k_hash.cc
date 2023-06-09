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

/* tresor includes */
#include <tresor/sha256_4k_hash.h>
#include <tresor/types.h>

/* base includes */
#include <util/string.h>

/* libcrypto */
#include <openssl/sha.h>


bool Tresor::check_sha256_4k_hash(Block const &blk,
                                  Hash  const &expected_hash)
{
	Hash got_hash;
	calc_sha256_4k_hash(blk, got_hash);
	return got_hash == expected_hash;
}


void Tresor::calc_sha256_4k_hash(Block const &blk,
                                 Hash        &hash)
{
	SHA256_CTX context { };
	if (!SHA256_Init(&context)) {
		class Calc_sha256_4k_hash_init_error { };
		throw Calc_sha256_4k_hash_init_error { };
	}
	if (!SHA256_Update(&context, &blk, BLOCK_SIZE)) {
		class Calc_sha256_4k_hash_update_error { };
		throw Calc_sha256_4k_hash_update_error { };
	}
	if (!SHA256_Final((unsigned char *)(&hash), &context)) {
		class Calc_sha256_4k_hash_final_error { };
		throw Calc_sha256_4k_hash_final_error { };
	}
}
