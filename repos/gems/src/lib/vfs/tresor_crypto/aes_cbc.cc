/*
 * \brief  Integration of the Tresor block encryption
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

/* base includes */
#include <base/log.h>
#include <util/string.h>

/* tresor includes */
#include <tresor/types.h>

/* vfs tresor crypt includes */
#include <aes_cbc_4k/aes_cbc_4k.h>
#include <interface.h>

namespace {

using namespace Genode;

struct Crypto : Tresor_crypto::Interface
{
	struct Buffer_size_mismatch    : Genode::Exception { };
	struct Key_value_size_mismatch : Genode::Exception { };

	struct {
		uint32_t        id   { };
		Aes_cbc_4k::Key key  { };
		bool            used { false };
	} keys [Slots::NUM_SLOTS];

	struct {
		struct crypt_ring {
			unsigned head { 0 };
			unsigned tail { 0 };

			struct {
				uint64_t      blk_nr { 0 };
				uint32_t      key_id { 0 };
				Tresor::Block data   { };
			} queue [4];

			unsigned max() const {
				return sizeof(queue) / sizeof(queue[0]); }

			bool acceptable() const {
				return ((head + 1) % max() != tail); }

			template <typename FUNC>
			bool enqueue(FUNC const &fn)
			{
				if (!acceptable())
					return false;

				fn(queue[head]);
				head = (head + 1) % max();

				return true;
			}

			template <typename FUNC>
			bool apply_crypt(FUNC const &fn)
			{
				if (head == tail)
					return false;

				if (!fn(queue[tail]))
					return false;

				tail = (tail + 1) % max();
				return true;
			}
		};

		struct crypt_ring encrypt;
		struct crypt_ring decrypt;

		template <typename FUNC>
		bool queue_encrypt(FUNC const &fn) { return encrypt.enqueue(fn); }

		template <typename FUNC>
		bool apply_encrypt(FUNC const &fn) { return encrypt.apply_crypt(fn); }

		template <typename FUNC>
		bool queue_decrypt(FUNC const &fn) { return decrypt.enqueue(fn); }

		template <typename FUNC>
		bool apply_decrypt(FUNC const &fn) { return decrypt.apply_crypt(fn); }
	} jobs { };

	template <typename FUNC>
	bool apply_to_unused_key(FUNC const &fn)
	{
		for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
			if (keys[i].used) continue;

			return fn(keys[i]);
		}
		return false;
	}

	template <typename FUNC>
	bool apply_key(uint32_t const id, FUNC const &fn)
	{
		for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
			if (!keys[i].used || id != keys[i].id) continue;

			return fn(keys[i]);
		}

		return false;
	}

	Crypto() { }

	/***************
	 ** interface **
	 ***************/

	bool execute() override
	{
		return true;
	}

	bool add_key(uint32_t const     id,
	             char const * const value,
	             size_t             value_len) override
	{
		return apply_to_unused_key([&](auto &key_slot) {
			if (value_len != sizeof(key_slot.key))
				return false;

			if (!_slots.store(id))
				return false;

			Genode::memcpy(key_slot.key.values, value, sizeof(key_slot.key));
			key_slot.id   = id;
			key_slot.used = true;

			return true;
		});
	}

	bool remove_key(uint32_t const id) override
	{
		return apply_key (id, [&] (auto &meta) {
			Genode::memset(meta.key.values, 0, sizeof(meta.key.values));

			meta.used = false;

			_slots.remove(id);
			return true;
		});
	}

	bool submit_encryption_request(uint64_t const  block_number,
	                               uint32_t const  key_id,
	                               Const_byte_range_ptr const &src) override
	{
		if (!src.start || src.num_bytes != sizeof (Tresor::Block)) {
			error("buffer has wrong size");
			throw Buffer_size_mismatch();
		}

		if (!jobs.encrypt.acceptable())
			return false;

		return apply_key (key_id, [&] (auto &meta) {
			return jobs.queue_encrypt([&] (auto &job) {
				job.blk_nr = block_number;
				job.key_id = key_id;

				uint64_t block_id = job.blk_nr;

				Aes_cbc_4k::Block_number     block_number { block_id };
				Aes_cbc_4k::Plaintext const &plaintext  = *reinterpret_cast<Aes_cbc_4k::Plaintext const *>(src.start);
				Aes_cbc_4k::Ciphertext      &ciphertext = *reinterpret_cast<Aes_cbc_4k::Ciphertext *>(&job.data);

				/* paranoia */
				static_assert(sizeof(plaintext) == sizeof(job.data), "size mismatch");

				Aes_cbc_4k::encrypt(meta.key, block_number, plaintext, ciphertext);
			});
		});
	}

	Complete_request encryption_request_complete(Byte_range_ptr const &dst) override
	{
		static_assert(sizeof(Tresor::Block) == sizeof(Aes_cbc_4k::Ciphertext), "size mismatch");
		static_assert(sizeof(Tresor::Block) == sizeof(Aes_cbc_4k::Plaintext), "size mismatch");

		if (dst.num_bytes != sizeof (Tresor::Block)) {
			error("buffer has wrong size");
			throw Buffer_size_mismatch();
		}

		uint64_t block_id = 0;

		bool const valid = jobs.apply_encrypt([&](auto const &job) {
			Genode::memcpy(dst.start, &job.data, sizeof(job.data));

			block_id = job.blk_nr;

			return true;
		});

		return Complete_request { .valid = valid,
		                          .block_number = block_id };
	}

	bool submit_decryption_request(uint64_t const  block_number,
	                               uint32_t const  key_id,
	                               Const_byte_range_ptr const &src) override
	{
		if (src.num_bytes != sizeof (Tresor::Block)) {
			error("buffer has wrong size");
			throw Buffer_size_mismatch();
		}

		if (!jobs.decrypt.acceptable())
			return false;

		/* use apply_key to make sure key_id is actually known */
		return apply_key (key_id, [&] (auto &) {
			return jobs.queue_decrypt([&] (auto &job) {
				job.blk_nr = block_number;
				job.key_id = key_id;
				Genode::memcpy(&job.data, src.start, sizeof(job.data));
			});
		});
	}

	Complete_request decryption_request_complete(Byte_range_ptr const &dst) override
	{
		static_assert(sizeof(Tresor::Block) == sizeof(Aes_cbc_4k::Ciphertext), "size mismatch");
		static_assert(sizeof(Tresor::Block) == sizeof(Aes_cbc_4k::Plaintext), "size mismatch");

		if (dst.num_bytes != sizeof (Tresor::Block)) {
			error("buffer has wrong size");
			throw Buffer_size_mismatch();
		}

		uint64_t block_id = 0;

		bool const valid = jobs.apply_decrypt([&](auto const &job) {
			bool ok = apply_key (job.key_id, [&] (auto &meta) {
				block_id = job.blk_nr;

				Aes_cbc_4k::Block_number      block_number { block_id };
				Aes_cbc_4k::Ciphertext const &ciphertext = *reinterpret_cast<Aes_cbc_4k::Ciphertext const *>(&job.data);
				Aes_cbc_4k::Plaintext        &plaintext  = *reinterpret_cast<Aes_cbc_4k::Plaintext *>(dst.start);

				/* paranoia */
				static_assert(sizeof(ciphertext) == sizeof(job.data), "size mismatch");

				Aes_cbc_4k::decrypt(meta.key, block_number, ciphertext, plaintext);

				return true;
			});

			return ok;
		});

		return Complete_request { .valid = valid,
		                          .block_number = block_id };
	}
};

} /* anonymous namespace */


Tresor_crypto::Interface &Tresor_crypto::get_interface()
{
	static Crypto inst;
	return inst;
}
