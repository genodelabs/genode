/*
 * \brief  Calculate and check hashes of tresor data blocks
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
#include <tresor/hash.h>
#include <tresor/types.h>

/* libcrypto */
#include <openssl/sha.h>

bool Tresor::check_hash(Block const &blk, Hash const &expected_hash)
{
	Hash got_hash;
	calc_hash(blk, got_hash);
	return got_hash == expected_hash;
}


void Tresor::calc_hash(Block const &blk, Hash &hash)
{
	SHA256_CTX context { };
	ASSERT(SHA256_Init(&context));
	ASSERT(SHA256_Update(&context, &blk, BLOCK_SIZE));
	ASSERT(SHA256_Final((unsigned char *)(&hash), &context));
}


Tresor::Hash Tresor::hash(Block const &blk)
{
	Hash hash { };
	calc_hash(blk, hash);
	return hash;
}
