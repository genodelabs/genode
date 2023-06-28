/*
 * \brief  Local variants of doing AES-256 and AES-256 key wrapping
 * \author Martin Stein
 * \date   2021-04-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AES_256_H_
#define _AES_256_H_

/* Genode includes */
#include <base/stdint.h>

namespace Aes_256
{
	void encrypt_with_zeroed_iv(unsigned char       *ciphertext_base,
	                            Genode::size_t       ciphertext_size,
	                            unsigned char const *plaintext_base,
	                            unsigned char const *key_base,
	                            Genode::size_t       key_size);

	void decrypt_with_zeroed_iv(unsigned char       *plaintext_base,
	                            Genode::size_t       plaintext_size,
	                            unsigned char const *ciphertext_base,
	                            unsigned char const *key_base,
	                            Genode::size_t       key_size);
}

namespace Aes_256_key_wrap
{
	enum { KEY_PLAINTEXT_SIZE = 32 };
	enum { CIPHERTEXT_SIZE = 40 };
	enum { KEY_ENCRYPTION_KEY_SIZE = 32 };

	/**
	 * Implementation of the "Key Wrap" algorithm (alternative indexing-based
	 * variant) defined in RFC 3394 "Advanced Encryption Standard (AES) Key
	 * Wrap Algorithm" paragraph 2.2.1, artificially tailored to a
	 * key-encryption-key (KEK) size of 256 bits and a key (key data) size of
	 * 256 bits.
	 */
	void wrap_key(Genode::uint64_t       *ciphertext_u64_ptr,
	              Genode::size_t          ciphertext_size,
	              Genode::uint64_t const *key_plaintext_u64_ptr,
	              Genode::size_t          key_plaintext_size,
	              Genode::uint64_t const *key_encryption_key_u64_ptr,
	              Genode::size_t          key_encryption_key_size);

	/**
	 * Implementation of the "Key Unwrap" algorithm (alternative indexing-based
	 * variant) defined in RFC 3394 "Advanced Encryption Standard (AES) Key
	 * Wrap Algorithm" paragraph 2.2.2, artificially tailored to a
	 * key-encryption-key (KEK) size of 256 bits and a key (key data) size of
	 * 256 bits.
	 */
	void unwrap_key(Genode::uint64_t       *key_plaintext_u64_ptr,
	                Genode::size_t          key_plaintext_size,
	                bool                   &key_plaintext_corrupt,
	                Genode::uint64_t const *ciphertext_u64_ptr,
	                Genode::size_t          ciphertext_size,
	                Genode::uint64_t const *key_encryption_key_u64_ptr,
	                Genode::size_t          key_encryption_key_size);
}

#endif /* _AES_256_H_ */
