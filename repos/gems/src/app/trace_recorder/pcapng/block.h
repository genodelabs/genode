/*
 * \brief  Generic type for PCAPNG blocks
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__BLOCK_H_
#define _PCAPNG__BLOCK_H_

/* Genode includes */
#include <base/fixed_stdint.h>

namespace Pcapng {
	struct Block_base;

	template <unsigned ID>
	struct Block;
}

struct Pcapng::Block_base
{
	/**
	 * Layout:   ----- 32-bit -----
	 *           |      Type      |
	 *           ------------------
	 *           |     Length     |
	 *           ------------------
	 *           |      ...       |
	 *           ------------------
	 *           |     Length     |
	 *           ------------------
	 */

	uint32_t const _type;
	uint32_t       _length  { 0 };

	static constexpr uint32_t padded_size(uint32_t hdr_sz)
	{
		/* add padding to 4-byte boundary */
		const uint32_t hdr_sz_padded = Genode::align_addr(hdr_sz, 2);

		return hdr_sz_padded;
	}

	static constexpr uint32_t block_size(uint32_t sz)
	{
		return padded_size(sz) + sizeof(uint32_t);
	}

	Block_base(uint32_t type)
	: _type(type)
	{ }

	void commit(uint32_t hdr_sz)
	{
		const uint32_t hdr_sz_padded = padded_size(hdr_sz);

		_length = hdr_sz_padded + sizeof(uint32_t);

		/* store length also after payload to support backward navigation */
		((uint32_t*)this)[hdr_sz_padded/4] = _length;
	}

	bool     has_type(uint32_t type) const { return _type == type; }
	uint32_t size()                  const { return _length; }

} __attribute__((packed));


template <unsigned TYPE_ID>
struct Pcapng::Block : Pcapng::Block_base
{
	Block()
	: Block_base(TYPE_ID)
	{ }

	static uint32_t type() { return TYPE_ID; };

} __attribute__((packed));

#endif /* _PCAPNG__BLOCK_H_ */
