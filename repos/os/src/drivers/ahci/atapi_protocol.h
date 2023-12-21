/*
 * \brief  ATAPI protocol driver
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AHCI__ATAPI_PROTOCOL_H_
#define _AHCI__ATAPI_PROTOCOL_H_

#include "ahci.h"
#include <util/endian.h>

namespace Atapi {
	class Protocol;
	using namespace Ahci;
	using namespace Genode;
}

class Atapi::Protocol : public Ahci::Protocol, Noncopyable
{
	private:

		Block::Request  _pending { };
		block_number_t  _block_count { 0 };
		size_t          _block_size { 2048 };

		void _atapi_command(Port &port, Port_mmio &mmio)
		{
			Command_header header(port.command_header_range(0));
			header.atapi_command();
			header.clear_byte_count();
			port.execute(0, mmio);
		}

		void _read_sense(Port &port, Port_mmio &mmio)
		{
			Command_table table(port.command_table_range(0),
			                    port.device_info_dma_addr, 0x1000);
			table.fis.atapi();
			table.atapi_cmd.read_sense();

			_atapi_command(port, mmio);
		}

		void _test_unit_ready(Port &port, Port_mmio &mmio)
		{
			Command_table table(port.command_table_range(0), 0, 0);
			table.fis.atapi();
			table.atapi_cmd.test_unit_ready();

			_atapi_command(port, mmio);
		}

		void _read_capacity(Port &port, Port_mmio &mmio)
		{
			Command_table table(port.command_table_range(0),
			                    port.device_info_dma_addr, 0x1000);
			table.fis.atapi();
			table.fis.byte_count(~0);

			table.atapi_cmd.read_capacity();

			_atapi_command(port, mmio);
		}

		void _start_unit(Port &port, Port_mmio &mmio)
		{
			Command_table table(port.command_table_range(0), 0, 0);
			table.fis.atapi();
			table.atapi_cmd.start_unit();

			_atapi_command(port, mmio);
		}

	public:

		/******************************
		 ** Ahci::Protocol interface **
		 ******************************/

		unsigned init(Port &port, Port_mmio &mmio) override
		{
			mmio.write<Port::Cmd::Atapi>(1);

			retry<Port::Polling_timeout>(
				[&] {

					_start_unit(port, mmio);
					mmio.wait_for_any(port.delayer,
					                  Port::Is::Dss::Equal(1), Port::Is::Pss::Equal(1),
					                  Port::Is::Dhrs::Equal(1));
					port.ack_irq(mmio);

					/* read sense */
					_read_sense(port, mmio);
					mmio.wait_for_any(port.delayer,
					                  Port::Is::Dss::Equal(1), Port::Is::Pss::Equal(1),
					                  Port::Is::Dhrs::Equal(1));
					port.ack_irq(mmio);

						/* test unit ready */
					_test_unit_ready(port, mmio);
					mmio.wait_for(port.delayer, Port::Is::Dhrs::Equal(1));
					port.ack_irq(mmio);

					Device_fis f(*port.fis);
					/* check if devic is ready */
					if (!f.read<Device_fis::Status::Device_ready>() || f.read<Device_fis::Error>())
						throw Port::Polling_timeout();

					_read_capacity(port, mmio);
					mmio.wait_for_any(port.delayer,
					                  Port::Is::Dss::Equal(1), Port::Is::Pss::Equal(1),
					                  Port::Is::Dhrs::Equal(1));
					port.ack_irq(mmio);

					_block_count = host_to_big_endian(((unsigned *)port.device_info->start)[0]) + 1;
					_block_size  = host_to_big_endian(((unsigned *)port.device_info->start)[1]);
				},
				[&] {}, 3);

			return 1;
		}

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size,
			         .block_count = _block_count,
			         .align_log2  = 1,
			         .writeable   = false };
		}

		void handle_irq(Port &port, Port_mmio &mmio) override
		{
			port.ack_irq(mmio);
		}

		void writeable(bool) override { }

		Response submit(Port &port, Block::Request const &request,
		                Port_mmio &mmio) override
		{
			if (request.operation.type != Block::Operation::Type::READ ||
			    port.sanity_check(request) == false || port.dma_base == 0)
				return Response::REJECTED;

			if (_pending.operation.valid())
				return Response::RETRY;

			Block::Operation op = request.operation;
			_pending = request;
			_pending.success = false;

			/* setup fis */
			Command_table table(port.command_table_range(0),
			                    port.dma_base + request.offset,
			                    op.count * _block_size);
			table.fis.atapi();

			/* setup atapi command */
			table.atapi_cmd.read10(op.block_number, op.count);
			table.fis.byte_count(~0);

			/* set and clear write flag in command header */
			Command_header header(port.command_header_range(0));
			header.write<Command_header::Bits::W>(0);
			header.clear_byte_count();

			port.execute(0, mmio);

			return Response::ACCEPTED;
		}

		Block::Request completed(Port_mmio &mmio) override
		{
			if (!_pending.operation.valid() || mmio.read<Port::Ci>())
				return Block::Request();

			Block::Request request = _pending;
			_pending.operation.type = Block::Operation::Type::INVALID;

			return request;
		}
};

#endif /* _AHCI__ATAPI_PROTOCOL_H_ */
