/*
 * \brief  Test for using libsparkcrypto
 * \author Norman Feske
 * \date   2018-12-20
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>

#include <aes_cbc_4k/aes_cbc_4k.h>

namespace Test {
	struct Main;
	using namespace Genode;
}


namespace Genode {

	static inline void print(Output &out, Aes_cbc_4k::Ciphertext const &data)
	{
		constexpr unsigned NUM_LINES      = 10;
		constexpr unsigned BYTES_PER_LINE = 32;

		for (unsigned line = 0; line < NUM_LINES; line++) {

			for (unsigned i = 0; i < BYTES_PER_LINE; i++)
				print(out, Genode::Hex(data.values[line*BYTES_PER_LINE + i],
				                       Genode::Hex::OMIT_PREFIX,
				                       Genode::Hex::PAD));
			if (line + 1 < NUM_LINES)
				print(out, "\n");
		}
	}
}


struct Test::Main
{
	Env &_env;

	Attached_rom_dataspace _plaintext { _env, "plaintext" };
	Attached_rom_dataspace _key       { _env, "key" };

	Aes_cbc_4k::Ciphertext _ciphertext { };
	Aes_cbc_4k::Plaintext  _decrypted_plaintext  { };

	Main(Env &env) : _env(env)
	{
		Aes_cbc_4k::Block_number const block_number { 0 };

		Aes_cbc_4k::Key       const &key       = *_key.local_addr<Aes_cbc_4k::Key>();
		Aes_cbc_4k::Plaintext const &plaintext = *_plaintext.local_addr<Aes_cbc_4k::Plaintext>();

		Aes_cbc_4k::encrypt(key, block_number, plaintext, _ciphertext);

		log("ciphertext:\n", _ciphertext);

		Aes_cbc_4k::decrypt(key, block_number, _ciphertext, _decrypted_plaintext);

		/* compare decrypted ciphertext with original plaintext */
		if (memcmp(plaintext.values, _decrypted_plaintext.values, sizeof(plaintext))) {
			error("plaintext differs from decrypted ciphertext");
			return;
		}

		log("Test succeeded");
	}
};


Genode::Env *__genode_env;


void Component::construct(Genode::Env &env)
{
	/* make ada-runtime happy */
	__genode_env = &env;
	env.exec_static_constructors();

	static Test::Main inst(env);
}
