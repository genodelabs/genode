/*
 * \brief  ATA protocol driver
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _AHCI__ATA_PROTOCOL_H_
#define _AHCI__ATA_PROTOCOL_H_

#include <base/log.h>
#include "ahci.h"
#include "util.h"

namespace Ata {
	struct Identity;
	template <typename DEVICE_STRING> struct String;
	class  Protocol;
	using namespace Genode;
	using namespace Ahci;
}

/**
 * Return data of 'identify_device' ATA command
 */
struct Ata::Identity : Mmio<0x1a4>
{
	using Mmio::Mmio;

	struct Serial_number : Register_array<0x14, 8, 20, 8> { };
	struct Model_number : Register_array<0x36, 8, 40, 8> { };

	struct Queue_depth : Register<0x96, 16>
	{
		struct Max_depth : Bitfield<0, 5> { };
	};

	struct Sata_caps   : Register<0x98, 16>
	{
		struct Ncq_support : Bitfield<8, 1> { };
	};

	struct Sector_count : Register<0xc8, 64> { };

	struct Logical_block  : Register<0xd4, 16>
	{
		struct Per_physical : Bitfield<0,  3> { }; /* 2^X logical per physical */
		struct Longer_512   : Bitfield<12, 1> { };
		struct Multiple     : Bitfield<13, 1> { }; /* multiple logical blocks per physical */
	};

	struct Logical_words : Register<0xea, 32> { }; /* words (16 bit) per logical block */

	struct Alignment : Register<0x1a2, 16>
	{
		struct Logical_offset : Bitfield<0, 14> { }; /* offset first logical block in physical */
	};

	void info()
	{
		log("  queue depth: ", read<Queue_depth::Max_depth>() + 1, " "
		    "ncq: ", read<Sata_caps::Ncq_support>());
		log("  numer of sectors: ", read<Sector_count>());
		log("  multiple logical blocks per physical: ",
		    read<Logical_block::Multiple>() ? "yes" : "no");
		log("  logical blocks per physical: ",
		    1U << read<Logical_block::Per_physical>());
		log("  logical block size is above 512 byte: ",
		    read<Logical_block::Longer_512>() ? "yes" : "no");
		log("  words (16bit) per logical block: ",
		    read<Logical_words>());
		log("  offset of first logical block within physical: ",
		    read<Alignment::Logical_offset>());
	}
};


/**
 * 16-bit word big endian device ASCII characters
 */
template <typename DEVICE_STRING>
struct Ata::String
{
	char buf[DEVICE_STRING::ITEMS + 1];

	String(Identity & info)
	{
		long j = 0;
		for (unsigned long i = 0; i < DEVICE_STRING::ITEMS; i++) {
			/* read and swap even and uneven characters */
			char c = (char)info.read<DEVICE_STRING>(i ^ 1);
			if (is_whitespace(c) && j == 0)
				continue;
			buf[j++] = c;
		}

		buf[j] = 0;

		/* remove trailing white spaces */
		while ((j > 0) && (buf[--j] == ' '))
			buf[j] = 0;
	}

	bool operator == (char const *other) const
	{
		return strcmp(buf, other) == 0;
	}

	void print(Output &out) const { print(out, (char const *)buf); }
	char const *cstring() { return buf; }
};


/**
 * Protocol driver using ncq- and non-ncq commands
 */
class Ata::Protocol : public Ahci::Protocol, Noncopyable
{
	private:

		bool _syncing { false };

		struct Request : Block::Request
		{
			bool valid() const { return operation.valid(); }
			void invalidate() { operation.type = Block::Operation::Type::INVALID; }

			Request & operator=(const Block::Request &request)
			{
				operation = request.operation;
				success = request.success;
				offset = request.offset;
				tag = request.tag;

				return *this;
			}
		};

		Util::Slots<Request, 32> _slots { };
		unsigned                 _slot_states = 0;

		typedef String<Identity::Serial_number> Serial_string;
		typedef String<Identity::Model_number>  Model_string;

		Constructible<Identity>      _identity { };
		bool                         _writeable { false };

	public:

		Constructible<Serial_string> serial { };
		Constructible<Model_string>  model  { };

	private:

		bool _overlap_check(Block::Request const &request)
		{
			block_number_t block_number = request.operation.block_number;
			block_number_t end = block_number + request.operation.count - 1;

			auto overlap_check = [&] (Request const &req) {
				if (req.operation.type == Block::Operation::Type::SYNC)
					return false;

				block_number_t pending_start = req.operation.block_number;
				block_number_t pending_end   = pending_start + req.operation.count - 1;

				/* check if a pending packet overlaps */
				if ((block_number >= pending_start && block_number <= pending_end) ||
				    (end          >= pending_start && end          <= pending_end) ||
				    (pending_start >= block_number && pending_start <= end) ||
				    (pending_end   >= block_number && pending_end   <= end)) {

					warning("overlap: ",
					        "pending ", pending_start,
					        " + ", req.operation.count,
									" (", req.operation.type == Block::Operation::Type::WRITE ?
									"write" : "read", "), ",
					        "request: ", block_number, " + ", request.operation.count,
									" (", request.operation.type == Block::Operation::Type::WRITE ?
									"write" : "read", ")");
					return true;
				}

				return false;
			};

			return _slots.for_each(overlap_check);
		}

		bool _ncq_support(Port &port)
		{
			return _identity->read<Identity::Sata_caps::Ncq_support>() && port.hba.ncq();
		}

		size_t _block_size() const
		{
			size_t size = 512;

			if (_identity->read<Identity::Logical_block::Longer_512>())
				size = _identity->read<Identity::Logical_words>() / 2;

			return size;
		}

		Block::sector_t _block_count() const
		{
			return _identity->read<Identity::Sector_count>();
		}

	public:

		/******************************
		 ** Ahci::Protocol interface **
		 ******************************/

		unsigned init(Port &port, Port_mmio &mmio) override
		{
			/* identify device */
			Command_table table(port.command_table_range(0),
			                    port.device_info_dma_addr, 0x1000);
			table.fis.identify_device();
			port.execute(0, mmio);

			mmio.wait_for_any(port.delayer, Port::Is::Dss::Equal(1),
			                                Port::Is::Pss::Equal(1),
			                                Port::Is::Dhrs::Equal(1));

			_identity.construct(*port.device_info);
			serial.construct(*_identity);
			model.construct(*_identity);

			if (verbose) {
				log("  model number: ",  Cstring(model->buf));
				log("  serial number: ", Cstring(serial->buf));
				_identity->info();
			}

			/* read number of command slots of ATA device */
			unsigned cmd_slots = _identity->read<Identity::Queue_depth::Max_depth >() + 1;

			/* no native command queueing */
			if (!_ncq_support(port))
				cmd_slots = 1;

			_slots.limit((size_t)cmd_slots);
			port.ack_irq(mmio);

			return cmd_slots;
		}

		void handle_irq(Port &port, Port_mmio &mmio) override
		{
			unsigned is = mmio.read<Port::Is>();

			/* ncg */
			if (_ncq_support(port) && Port::Is::Fpdma_irq::get(is))
				do {
					port.ack_irq(mmio);
				} while (Port::Is::Sdbs::get(mmio.read<Port::Is>()));
			/* normal dma */
			else if (Port::Is::Dma_ext_irq::get(mmio.read<Port::Is>()))
				port.ack_irq(mmio);

			_slot_states = mmio.read<Port::Ci>() | mmio.read<Port::Sact>();
			port.stop(mmio);

			_syncing = false;
		}

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size(),
			         .block_count = _block_count(),
			         .align_log2  = log2(2ul),
			         .writeable   = _writeable };
		}

		void writeable(bool rw) override { _writeable = rw; }

		Response submit(Port &port, Block::Request const &request,
		                Port_mmio &mmio) override
		{
			Block::Operation const op = request.operation;

			bool const sync  = (op.type == Block::Operation::Type::SYNC);
			bool const write = (op.type == Block::Operation::Type::WRITE);

			if ((sync && _slot_states) || _syncing)
				return Response::RETRY;

			if (_writeable == false && write)
				return Response::REJECTED;

			if (Block::Operation::has_payload(op.type)) {
				if (port.sanity_check(request) == false || port.dma_base == 0)
					return Response::REJECTED;

				if (_overlap_check(request))
					return Response::RETRY;
			}

			Request *r = _slots.get();

			if (r == nullptr)
				return Response::RETRY;

			*r = request;

			size_t slot         = _slots.index(*r);
			_slot_states       |= 1u << slot;

			/* setup fis */
			Command_table table(port.command_table_range(slot),
			                    port.dma_base + request.offset, /* physical address */
			                    op.count * _block_size());

			/* setup ATA command */
			if (sync) {
				table.fis.flush_cache_ext();
				_syncing = true;
			} else if (_ncq_support(port)) {
				table.fis.fpdma(write == false, op.block_number, op.count, slot);
				/* ensure that 'Cmd::St' is 1 before writing 'Sact' */
				port.start(mmio);
				/* set pending */
				mmio.write<Port::Sact>(1U << slot);
			} else {
				table.fis.dma_ext(write == false, op.block_number, op.count);
			}

			/* set or clear write flag in command header */
			Command_header header(port.command_header_range(slot));
			header.write<Command_header::Bits::W>(write ? 1 : 0);
			header.clear_byte_count();

			port.execute(slot, mmio);

			return Response::ACCEPTED;
		}

		Block::Request completed(Port_mmio &) override
		{
			Block::Request r { };

			_slots.for_each([&](Request &request)
			{
				size_t index = _slots.index(request);
				/* request still pending */
				if (_slot_states & (1u << index))
					return false;

				r = request;
				request.invalidate();

				return true;
			});

			return r;
		}

		bool pending_requests() const override { return !!_slot_states; }
};

#endif /* _AHCI__ATA_PROTOCOL_H_ */
