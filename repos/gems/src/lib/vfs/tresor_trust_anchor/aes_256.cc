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

/* Genode includes */
#include <util/string.h>

/* OpenSSL includes */
namespace Openssl {

	#include <openssl/aes.h>
}

/* local includes */
#include <aes_256.h>
#include <integer.h>

using namespace Genode;


namespace Aes_256
{
	struct Initialization_vector
	{
		unsigned char values[16];
	};
}

namespace Aes_256_key_wrap {

	enum { KEY_PLAINTEXT_NR_OF_64_BIT_VALUES = KEY_PLAINTEXT_SIZE / 8 };
	enum { NR_OF_WRAPPING_STEPS = 6 };
	enum { INTEGRITY_CHECK_VALUE = 0xa6a6a6a6a6a6a6a6 };
}


template <typename OBJECT_TYPE>
static void inline overwrite_object_with_zeroes(OBJECT_TYPE &object)
{
	memset(&object, 0, sizeof(object));

	/* trigger compiler to not drop the memset */
	asm volatile(""::"r"(&object):"memory");
}


void Aes_256::encrypt_with_zeroed_iv(unsigned char       *ciphertext_base,
                                     size_t               ciphertext_size,
                                     unsigned char const *plaintext_base,
                                     unsigned char const *key_base,
                                     size_t               key_size)
{
	Openssl::AES_KEY aes_key;
	if (AES_set_encrypt_key(key_base, (int)(key_size * 8), &aes_key)) {
		class Failed_to_set_key { };
		throw Failed_to_set_key { };
	}
	Aes_256::Initialization_vector iv { };
	memset(iv.values, 0, sizeof(iv.values));
	AES_cbc_encrypt(
		plaintext_base, ciphertext_base, ciphertext_size, &aes_key, iv.values,
		AES_ENCRYPT);

	/* ensure that relevant encryption data doesn't remain on the stack */
	overwrite_object_with_zeroes(aes_key);
}


void Aes_256::decrypt_with_zeroed_iv(unsigned char       *plaintext_base,
                                     size_t               plaintext_size,
                                     unsigned char const *ciphertext_base,
                                     unsigned char const *key_base,
                                     size_t               key_size)
{
	Openssl::AES_KEY aes_key;
	if (AES_set_decrypt_key(key_base, (int)(key_size * 8), &aes_key)) {
		class Failed_to_set_key { };
		throw Failed_to_set_key { };
	}
	Aes_256::Initialization_vector iv { };
	memset(iv.values, 0, sizeof(iv.values));
	AES_cbc_encrypt(
		ciphertext_base, plaintext_base, plaintext_size, &aes_key, iv.values,
		AES_DECRYPT);

	/* ensure that relevant encryption data doesn't remain on the stack */
	overwrite_object_with_zeroes(aes_key);
}


void Aes_256_key_wrap::wrap_key(uint64_t       *ciphertext_u64_ptr,
                                size_t          ciphertext_size,
                                uint64_t const *key_plaintext_u64_ptr,
                                size_t          key_plaintext_size,
                                uint64_t const *key_encryption_key_u64_ptr,
                                size_t          key_encryption_key_size)
{
	if (ciphertext_size != CIPHERTEXT_SIZE) {
		class Bad_ciphertext_size { };
		throw Bad_ciphertext_size { };
	}
	if (key_plaintext_size != KEY_PLAINTEXT_SIZE) {
		class Bad_key_plaintext_size { };
		throw Bad_key_plaintext_size { };
	}
	if (key_encryption_key_size != KEY_ENCRYPTION_KEY_SIZE) {
		class Bad_key_encryption_key_size { };
		throw Bad_key_encryption_key_size { };
	}
	ciphertext_u64_ptr[0] = INTEGRITY_CHECK_VALUE;
	memcpy(
		&ciphertext_u64_ptr[1], &key_plaintext_u64_ptr[0],
		ciphertext_size - sizeof(ciphertext_u64_ptr[0]));

	for (unsigned step_idx = 0;
	     step_idx < NR_OF_WRAPPING_STEPS;
	     step_idx++) {

		for (unsigned value_idx = 1;
		     value_idx <= KEY_PLAINTEXT_NR_OF_64_BIT_VALUES;
		     value_idx++) {

			uint64_t encryption_input[2];
			encryption_input[0] = ciphertext_u64_ptr[0];
			encryption_input[1] = ciphertext_u64_ptr[value_idx];

			uint64_t encryption_output[2];

			Aes_256::encrypt_with_zeroed_iv(
				(unsigned char *)encryption_output,
				sizeof(encryption_output),
				(unsigned char *)encryption_input,
				(unsigned char *)key_encryption_key_u64_ptr,
				key_encryption_key_size);

			uint64_t const xor_operand {
				Integer::u64_swap_byte_order(
					((uint64_t)KEY_PLAINTEXT_NR_OF_64_BIT_VALUES * step_idx) +
					value_idx) };

			ciphertext_u64_ptr[0] = encryption_output[0] ^ xor_operand;
			ciphertext_u64_ptr[value_idx] = encryption_output[1];
		}
	}
}


void Aes_256_key_wrap::unwrap_key(uint64_t       *key_plaintext_u64_ptr,
                                  size_t          key_plaintext_size,
                                  bool           &key_plaintext_corrupt,
                                  uint64_t const *ciphertext_u64_ptr,
                                  size_t          ciphertext_size,
                                  uint64_t const *key_encryption_key_u64_ptr,
                                  size_t          key_encryption_key_size)
{
	if (key_plaintext_size != KEY_PLAINTEXT_SIZE) {
		class Bad_key_plaintext_size { };
		throw Bad_key_plaintext_size { };
	}
	if (ciphertext_size != CIPHERTEXT_SIZE) {
		class Bad_ciphertext_size { };
		throw Bad_ciphertext_size { };
	}
	if (key_encryption_key_size != KEY_ENCRYPTION_KEY_SIZE) {
		class Bad_key_encryption_key_size { };
		throw Bad_key_encryption_key_size { };
	}
	uint64_t integrity_check_value { ciphertext_u64_ptr[0] };
	memcpy(&key_plaintext_u64_ptr[0], &ciphertext_u64_ptr[1], key_plaintext_size);

	for (signed step_idx = NR_OF_WRAPPING_STEPS - 1;
	     step_idx >= 0;
	     step_idx--) {

		for (unsigned value_idx = KEY_PLAINTEXT_NR_OF_64_BIT_VALUES;
		     value_idx >= 1;
		     value_idx--) {

			uint64_t const xor_operand {
				Integer::u64_swap_byte_order(
					((uint64_t)KEY_PLAINTEXT_NR_OF_64_BIT_VALUES * step_idx) +
					value_idx) };

			uint64_t encryption_input[2];
			encryption_input[0] = integrity_check_value ^ xor_operand;
			encryption_input[1] = key_plaintext_u64_ptr[value_idx - 1];

			uint64_t encryption_output[2];

			Aes_256::decrypt_with_zeroed_iv(
				(unsigned char *)encryption_output,
				sizeof(encryption_output),
				(unsigned char *)encryption_input,
				(unsigned char *)key_encryption_key_u64_ptr,
				key_encryption_key_size);

			integrity_check_value = encryption_output[0];
			key_plaintext_u64_ptr[value_idx - 1] = encryption_output[1];
		}
	}
	if (integrity_check_value == INTEGRITY_CHECK_VALUE) {
		key_plaintext_corrupt = false;
	} else {
		key_plaintext_corrupt = true;
	}
}
