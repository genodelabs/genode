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

	void calc_sha256_4k_hash(void const *data_ptr,
	                         void       *hash_ptr);


	bool check_sha256_4k_hash(void const *data_ptr,
	                          void const *exp_hash_ptr);
}

#endif /* _TRESOR__SHAE256_4K_HASH_ */
