/*
 * \brief  Block driver
 * \author Josef Söntgen
 * \date   2014-04-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BLOCK_DRIVER_H_
#define _BLOCK_DRIVER_H_

/* General includes */
#include <base/log.h>
#include <block_session/connection.h>
#include <block/component.h>
#include <os/packet_allocator.h>

/* local includes */
#include "cgd.h"


class Driver : public Block::Driver
{
	private:

		Genode::Heap                      &_heap;
		Block::Session::Operations         _ops;
		Genode::size_t                     _blk_sz;
		Block::sector_t                    _blk_cnt;

		Cgd::Device                       *_cgd_device;


	public:

		Driver(Genode::Env &env, Genode::Heap &heap)
		:
			Block::Driver(env.ram()), _heap(heap),
			_blk_sz(0), _blk_cnt(0), _cgd_device(0)
		{
			try {
				_cgd_device = Cgd::init(_heap, env);
			} catch (...) {
				Genode::error("could not initialize cgd device.");
				throw Genode::Service_denied();
			}

			_blk_cnt = _cgd_device->block_count();
			_blk_sz  = _cgd_device->block_size();

			/*
			 * XXX We need write access to satisfy RUMP but we have to check
			 * the client policy in the session interface.
			 */
			_ops.set_operation(Block::Packet_descriptor::READ);
			_ops.set_operation(Block::Packet_descriptor::WRITE);
		}

		~Driver()
		{
			Cgd::deinit(_heap, _cgd_device);
		}

		bool _range_valid(Block::sector_t num, Genode::size_t count)
		{
			if (num + count > _blk_cnt) {
				Genode::error("requested block ", num, "-", num + count, " out of range!");
				return false;
			}

			return true;
		}


		/****************************
		 ** Block-driver interface **
		 ****************************/

		Genode::size_t  block_size()     { return _blk_sz;  }
		Block::sector_t block_count()    { return _blk_cnt; }
		Block::Session::Operations ops() { return _ops;     }

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char*                     buffer,
		          Block::Packet_descriptor &packet)
		{
			if (!_ops.supported(Block::Packet_descriptor::READ))
				throw Io_error();

			if(!_range_valid(block_number, block_count))
				throw Io_error();

			_cgd_device->read(buffer, block_count * _blk_sz, block_number * _blk_sz);
			ack_packet(packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           const char *              buffer,
		           Block::Packet_descriptor &packet)
		{
			if (!_ops.supported(Block::Packet_descriptor::WRITE))
				throw Io_error();

			if(!_range_valid(block_number, block_count))
				throw Io_error();

			_cgd_device->write(buffer, block_count * _blk_sz, block_number * _blk_sz);
			ack_packet(packet);
		}

		void sync() { }
};

#endif /* _BLOCK_DRIVER_H_ */
