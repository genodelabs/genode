/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* cbe_crypto includes */
#include <cbe/crypto/interface.h>

namespace {

using namespace Genode;

struct Crypto : Cbe_crypto::Interface
{
	char _internal_buffer[Cbe_crypto::BLOCK_SIZE] { };

	struct Request
	{
		uint64_t block_number;
		bool     pending;
	};

	Request _request { 0, false };

	Crypto() { }

	/***************
	 ** interface **
	 ***************/

	bool execute() override
	{
		return false;
	}

	bool add_key(uint32_t const  id, char const *, size_t) override
	{
		if (!_slots.store(id)) {
			return false;
		}

		log("Add key: id " , id);
		return true;
	}

	bool remove_key(uint32_t const id) override
	{
		log("Remove key: id " , id);
		_slots.remove(id);
		return true;
	}

	bool _submit_request(uint64_t const  block_number,
	                     uint32_t const  /* key_id */,
	                     char     const *src,
	                     size_t   const  src_len)
	{
		if (_request.pending) {
			return false;
		}

		if (src_len < sizeof (_internal_buffer)) {
			error("buffer too small");
			throw Buffer_too_small();
		}

		_request.pending      = true;
		_request.block_number = block_number;

		Genode::memcpy(_internal_buffer, src, sizeof (_internal_buffer));
		return true;
	}

	bool submit_encryption_request(uint64_t const  block_number,
	                               uint32_t const  key_id,
	                               char     const *src,
	                               size_t   const  src_len) override
	{
		return _submit_request(block_number, key_id, src, src_len);
	}

	Complete_request _request_complete(char *dst, size_t const dst_len)
	{
		if (!_request.pending) {
			return Complete_request { .valid = false, .block_number = 0 };
		}

		if (dst_len < sizeof (_internal_buffer)) {
			error("buffer too small");
			throw Buffer_too_small();
		}

		Genode::memcpy(dst, _internal_buffer, sizeof (_internal_buffer));

		_request.pending = false;

		return Complete_request {
			.valid        = true,
			.block_number = _request.block_number };
	}

	Complete_request encryption_request_complete(char *dst, size_t const dst_len) override
	{
		return _request_complete(dst, dst_len);
	}

	bool submit_decryption_request(uint64_t const  block_number,
	                               uint32_t const  key_id,
	                               char     const *src,
	                               size_t   const  src_len) override
	{
		return _submit_request(block_number, key_id, src, src_len);
	}

	Complete_request decryption_request_complete(char *dst, size_t dst_len) override
	{
		return _request_complete(dst, dst_len);
	}
};

} /* anonymous namespace */


Cbe_crypto::Interface &Cbe_crypto::get_interface()
{
	static Crypto inst;
	return inst;
}
