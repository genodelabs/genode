/*
 * \brief  Using libcrypto/openssl to implement aes_cbc_4k interface
 * \author Alexander Boettcher
 * \date   2020-09-24
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/string.h>

#include <aes_cbc_4k/aes_cbc_4k.h>

#include <openssl/aes.h>
#include <openssl/sha.h>

namespace Aes_cbc
{
	/* an enum by the OpenSSL library about the size of IV would be nice ! */
	struct Iv {
		unsigned char values[16];
	};

	struct Sn {
		unsigned char values[sizeof(Iv::values)] { }; /* zero initialized */

		Sn(Aes_cbc_4k::Block_number const &nr) {
			reinterpret_cast<Genode::uint64_t &>(values) = nr.value; }
	};

	struct Hash {
		unsigned char values[SHA256_DIGEST_LENGTH];
	};
};

static bool hash_key(Aes_cbc_4k::Key const &key, Aes_cbc::Hash &hash)
{
	static_assert(sizeof(key.values) == sizeof(hash.values),
	              "-key- size vs -hash- size mismatch");

	SHA256_CTX context;
	if (!SHA256_Init(&context))
		return false;

	if (!SHA256_Update(&context, key.values, sizeof(key.values)))
		return false;

	if (!SHA256_Final(hash.values, &context))
		return false;

	return true;
}

/*
 * clean up crypto temporary crypto data on stack
 */
template <typename T>
static void inline cleanup_crypto_data(T &t)
{
	Genode::memset(&t,  0, sizeof(t));
	/* trigger compiler to not drop the memset */
	asm volatile(""::"r"(&t):"memory");
}

template <typename T, typename S>
static void inline cleanup_crypto_data(T &t, S &s)
{
	Genode::memset(&t,  0, sizeof(t));
	Genode::memset(&s,  0, sizeof(s));
	/* trigger compiler to not drop the memsets */
	asm volatile(""::"r"(&t),"r"(&s):"memory");
}

/**
 * Calculate initialization vector (IV) according to
 * "Encrypted salt-sector initialization vector" (ESSIV) algorithm
 * by Clemens Fruhwirth (July 18, 2005) published in
 * "New Methods in Hard Disk Encryption" paper.
 */
static bool calculate_iv(Aes_cbc_4k::Key          const &key,
                         Aes_cbc_4k::Block_number const &block,
                         Aes_cbc::Iv                    &cipher_iv)
{
	Aes_cbc::Hash hash_of_key;
	if (!hash_key(key, hash_of_key))
		return false;

	AES_KEY key_for_iv;
	if (AES_set_encrypt_key(hash_of_key.values, sizeof(hash_of_key.values) * 8,
	                        &key_for_iv)) {
		/* clean up crypto relevant data which stays otherwise on stack */
		cleanup_crypto_data(hash_of_key);
		return false;
	}

	Aes_cbc::Sn const plain  { block };
	Aes_cbc::Iv       ivec   { };       /* zero IV */

	static_assert(sizeof(plain.values) == sizeof(cipher_iv.values),
	              "-plain- size vs -iv- size mismatch");

	AES_cbc_encrypt(plain.values, cipher_iv.values, sizeof(plain.values),
	                &key_for_iv, ivec.values, AES_ENCRYPT);

	cleanup_crypto_data(hash_of_key, key_for_iv);

	return true;
}

void Aes_cbc_4k::encrypt(Key const &key, Block_number const block_number,
                         Plaintext const &plain, Ciphertext &cipher)
{
	static_assert(sizeof(plain.values)  == 4096, "Plain text size mismatch");
	static_assert(sizeof(cipher.values) == 4096, "Cipher size mismatch");
	static_assert(sizeof(key.values)    ==   32, "Key size mismatch");

	AES_KEY aes_key;
	if (AES_set_encrypt_key(reinterpret_cast<unsigned char const *>(key.values),
	                        sizeof(key.values) * 8, &aes_key)) {
		Genode::error("setting encrypt key");
		return;
	}

	Aes_cbc::Iv iv;
	if (!calculate_iv(key, block_number, iv)) {
		Genode::error("iv calculation");
		cleanup_crypto_data(aes_key);
		return;
	}

	AES_cbc_encrypt(reinterpret_cast<unsigned char const *>(plain.values),
	                reinterpret_cast<unsigned char *>(cipher.values),
	                sizeof(cipher.values), &aes_key, iv.values, AES_ENCRYPT);

	/* clean up crypto relevant data which stays otherwise on stack */
	cleanup_crypto_data(aes_key, iv);
}

void Aes_cbc_4k::decrypt(Key const &key, Block_number const block_number,
                         Ciphertext const &cipher, Plaintext &plain)
{
	AES_KEY aes_key;
	if (AES_set_decrypt_key(reinterpret_cast<unsigned char const *>(key.values),
	                        sizeof(key.values) * 8, &aes_key)) {
		Genode::error("setting decrypt key");
		return;
	}

	Aes_cbc::Iv iv;
	if (!calculate_iv(key, block_number, iv)) {
		Genode::error("iv calculation");
		cleanup_crypto_data(aes_key);
		return;
	}

	AES_cbc_encrypt(reinterpret_cast<unsigned char const *>(cipher.values),
	                reinterpret_cast<unsigned char *>(plain.values),
	                sizeof(plain.values), &aes_key, iv.values, AES_DECRYPT);

	cleanup_crypto_data(aes_key, iv);
}
