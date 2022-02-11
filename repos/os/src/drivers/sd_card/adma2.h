/*
 * \brief  Advanced DMA 2
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ADMA2_H_
#define _ADMA2_H_

/* Genode includes */
#include <util/register.h>
#include <platform_session/dma_buffer.h>

namespace Adma2
{
	using namespace Genode;

	class Desc;
	class Table;
}

/**
 * Descriptor layout
 */
struct Adma2::Desc : Register<64>
{
	struct Valid   : Bitfield<0, 1> { };
	struct End     : Bitfield<1, 1> { };
	struct Int     : Bitfield<2, 1> { };
	struct Act1    : Bitfield<4, 1> { };
	struct Act2    : Bitfield<5, 1> { };
	struct Length  : Bitfield<16, 16>
	{
		/*
		 * According to the 'SD Specifications, Part A2, SD Host
		 * Controller, Simplified Specification, Version 2.00, February 8,
		 * 2007, Table 1-10', a maximum length of 65536 bytes is achieved
		 * by value 0. However, if we do so, the completion host-signal
		 * times out now and then. Thus, we use the next lower possible
		 * value.
		 */
		static constexpr addr_t align_log2 = 2;
		static constexpr size_t max        = (1 << WIDTH) - (1 << align_log2);
	};
	struct Address : Bitfield<32, 32> { };
};


/**
 * Descriptor table
 */
class Adma2::Table
{
	private:

		static size_t constexpr _max_desc = 1024;
		static size_t constexpr _ds_size  = _max_desc * sizeof(Desc::access_t);

		Platform::Dma_buffer   _ds;
		Desc::access_t * const _base_virt;

		/*
		 * Noncopyable
		 */
		Table(Table const &);
		Table &operator = (Table const &);

	public:

		Table(Platform::Connection &platform);

		/**
		 * Marshal descriptors according to block request
		 *
		 * \param size         request size in bytes
		 * \param buffer_phys  physical base of transfer buffer
		 *
		 * \retval  0  success
		 * \retval -1  error
		 */
		int setup_request(size_t const size, addr_t const buffer_phys);

		/*
		 * Accessors
		 */

		addr_t base_dma() const { return _ds.dma_addr(); }
};

#endif /* _ADMA2_H_ */
