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

#ifndef _TRESOR__SHAE256_4K_HASH_H_
#define _TRESOR__SHAE256_4K_HASH_H_

namespace Tresor {

	class Block;
	class Hash;

	void calc_sha256_4k_hash(Block const &blk,
	                         Hash        &hash);


	bool check_sha256_4k_hash(Block const &blk,
	                          Hash  const &expected_hash);
}

#endif /* _TRESOR__SHAE256_4K_HASH_ */
