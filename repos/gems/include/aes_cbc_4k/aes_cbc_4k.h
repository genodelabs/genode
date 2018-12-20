/*
 * \brief  Interface for AES CBC encryption/decrytion of 4KiB data blocks
 * \author Norman Feske
 * \date   2018-12-20
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AES_CBC_4K_H_
#define _AES_CBC_4K_H_

namespace Aes_cbc_4k {

	struct Key   { char values[32];   };
	struct Block { char values[4096]; };

	struct Plaintext  : Block { };
	struct Ciphertext : Block { };

	struct Block_number { Genode::uint64_t value; };

	void encrypt(Key const &, Block_number, Plaintext  const &, Ciphertext &);
	void decrypt(Key const &, Block_number, Ciphertext const &, Plaintext  &);
}

#endif /* _AES_CBC_4K_H_ */
