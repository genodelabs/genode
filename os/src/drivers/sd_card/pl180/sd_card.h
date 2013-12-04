/*
 * \brief  SD-card protocol
 * \author Christian Helmuth
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

#include <block/driver.h>

#include "host_driver.h"


class Sd_card : public Block::Driver
{
	private:

		Host_driver &_hd;

		enum { BLOCK_SIZE = 512 };

	public:

		Sd_card(Host_driver &host_driver)
		: _hd(host_driver)
		{
			unsigned resp;

			/* CMD0: go idle state */
			_hd.request(0, 0);

			/*
			 * CMD8: send interface condition
			 *
			 * XXX only one hard-coded value currently.
			 */
			_hd.request(8, 0x1aa, &resp);

			/*
			 * ACMD41: card send operating condition
			 *
			 * This is an application-specific command and, therefore, consists
			 * of prefix command CMD55 + CMD41.
			 */
			_hd.request(55, 0, &resp);
			_hd.request(41, 0x4000, &resp);

			/* CMD2: all send card identification (CID) */
			_hd.request(2, &resp);

			/* CMD3: send relative card address (RCA) */
			_hd.request(3, &resp);
			unsigned short rca = resp >> 16;

			/*
			 * Now, the card is in transfer mode...
			 */

			/* CMD7: select card */
			_hd.request(7, rca << 16, &resp);
		}

		Host_driver &host_driver() { return _hd; }

		/****************************
		 ** Block-driver interface **
		 ****************************/

		Genode::size_t block_size()  { return BLOCK_SIZE; }
		/*
		 * TODO report (and support) real capacity not just 512M
		 */
		Block::sector_t block_count() { return 0x20000000 /  BLOCK_SIZE; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			o.set_operation(Block::Packet_descriptor::WRITE);
			return o;
		}

		void read(Block::sector_t block_number,
		          Genode::size_t  block_count,
		          char           *out_buffer,
		          Block::Packet_descriptor &packet)
		{
			unsigned resp;
			unsigned length = BLOCK_SIZE;

			for (Genode::size_t i = 0; i < block_count; ++i) {
				/*
				 * CMD17: read single block
				 *
				 * SDSC cards use a byte address as argument while SDHC/SDSC uses a
				 * block address here.
				 */
				_hd.read_request(17, (block_number + i) * BLOCK_SIZE,
				                 length, &resp);
				_hd.read_data(length, out_buffer + (i * BLOCK_SIZE));
			}
			session->complete_packet(packet);
		}

		void write(Block::sector_t block_number,
		           Genode::size_t  block_count,
		           char const     *buffer,
		           Block::Packet_descriptor &packet)
		{
			unsigned resp;
			unsigned length = BLOCK_SIZE;

			for (Genode::size_t i = 0; i < block_count; ++i) {
				/*
				 * CMD24: write single block
				 *
				 * SDSC cards use a byte address as argument while SDHC/SDSC uses a
				 * block address here.
				 */
				_hd.write_request(24, (block_number + i) * BLOCK_SIZE,
				                  length, &resp);
				_hd.write_data(length, buffer + (i * BLOCK_SIZE));
			}
			session->complete_packet(packet);
		}
};

#endif /* _SD_CARD_H_ */
