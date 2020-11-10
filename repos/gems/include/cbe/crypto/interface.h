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

#ifndef _INCLUDE__CBE__CRYPTO__INTERFACE_H_
#define _INCLUDE__CBE__CRYPTO__INTERFACE_H_

/* Genode includes */
#include <base/exception.h>
#include <base/stdint.h>


namespace Cbe_crypto {

	using uint32_t = Genode::uint32_t;
	using uint64_t = Genode::uint64_t;
	using size_t   = Genode::size_t;

	struct Interface;

	Interface &get_interface();

	enum { BLOCK_SIZE = 4096u };

} /* namespace Cbe_crypto */


struct Cbe_crypto::Interface
{
	struct Buffer_too_small        : Genode::Exception { };
	struct Key_value_size_mismatch : Genode::Exception { };

	struct Complete_request
	{
		bool     const valid;
		uint64_t const block_number;
	};

	struct Slots
	{
		enum { NUM_SLOTS = 2, };
		uint32_t _store[NUM_SLOTS] { };

		bool store(uint32_t const id)
		{
			for (uint32_t &slot : _store) {
				if (slot == 0) {
					slot = id;
					return true;
				}
			}
			return false;
		}

		void remove(uint32_t const id)
		{
			for (uint32_t &slot : _store) {
				if (slot == id) {
					slot = 0;
					return;
				}
			}
		}

		template <typename FN>
		void for_each_key(FN const &func)
		{
			for (uint32_t const slot : _store) {
				if (slot != 0) {
					func(slot);
				}
			}
		}
	};

	Slots _slots { };

	virtual ~Interface() { }

	template <typename FN>
	void for_each_key(FN const &func)
	{
		_slots.for_each_key(func);
	}

	virtual bool execute() = 0;

	virtual bool add_key(uint32_t const  id,
	                     char const     *value,
	                     size_t          value_len) = 0;

	virtual bool remove_key(uint32_t const id) = 0;

	virtual bool submit_encryption_request(uint64_t const  block_number,
	                                       uint32_t const  key_id,
	                                       char     const *src,
	                                       size_t   const  src_len) = 0;

	virtual Complete_request encryption_request_complete(char *dst, size_t const dst_len) = 0;

	virtual bool submit_decryption_request(uint64_t const  block_number,
	                                       uint32_t const  key_id,
	                                       char     const *src,
	                                       size_t   const  src_len) = 0;

	virtual Complete_request decryption_request_complete(char *dst, size_t dst_len) = 0;
};

#endif /* _INCLUDE__CBE__CRYPTO__INTERFACE_H_ */
