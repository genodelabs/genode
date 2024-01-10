/*
 * \brief  SCSI Block Commands
 * \author Josef Soentgen
 * \date   2016-02-08
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCSI_H_
#define _SCSI_H_

#include <base/stdint.h>
#include <util/endian.h>
#include <util/mmio.h>

namespace Scsi {

	using namespace Genode;

	/*******************
	 ** Endian helper **
	 *******************/

	template <typename T>
	T be(T val)
	{
		uint8_t * p = reinterpret_cast<uint8_t *>(&val);
		T ret = 0;
		for (size_t i = 0; i < sizeof(T); i++)
			ret |= (T) (p[i] << ((sizeof(T)-i-1)*8));
		return ret;
	}


	/******************
	 * SCSI commands **
	 ******************/

	enum Opcode {
		TEST_UNIT_READY  = 0x00,
		REQUEST_SENSE    = 0x03,
		INQUIRY          = 0x12,
		START_STOP       = 0x1B,
		READ_CAPACITY_10 = 0x25,
		READ_10          = 0x28,
		WRITE_10         = 0x2a,
		READ_16          = 0x88,
		WRITE_16         = 0x8a,
		READ_CAPACITY_16 = 0x9e,
	};

	struct Inquiry_response;
	struct Request_sense_response;
	struct Capacity_response_10;
	struct Capacity_response_16;
	struct Start_stop_response;

	struct Cmd_6;
	struct Test_unit_ready;
	struct Request_sense;
	struct Inquiry;
	struct Start_stop;

	struct Cmd_10;
	struct Read_capacity_10;
	struct Io_10;
	struct Read_10;
	struct Write_10;

	struct Cmd_16;
	struct Read_capacity_16;
	struct Io_16;
	struct Read_16;
	struct Write_16;
}


/***************************
 * SCSI command responses **
 ***************************/

struct Scsi::Inquiry_response : Genode::Mmio<0x24>
{
	/*
	 * Minimum response length is 36 bytes.
	 *
	 * Some devices have problems when more is requested, e.g.: 
	 * - hama sd card reader (05e3:0738)
	 * - delock sata adapter (174c:5106)
	 */
	enum { LENGTH = 36 };

	struct Dt  : Register<0x0, 8> { }; /* device type */
	struct Rm  : Register<0x1, 8> { struct Rmb : Bitfield<7, 1> { }; }; /* removable media */
	struct Ver : Register<0x2, 8> { }; /* version */
	struct Rdf : Register<0x3, 8> { }; /* response data format */
	struct Al  : Register<0x4, 8> { }; /* additional ength */
	struct Flg : Register<0x7, 8> { }; /* flags */
	struct Vid : Register_array<0x8, 8, 8, 8> { }; /* vendor identification */
	struct Pid : Register_array<0x10, 8, 16, 8> { }; /* product identification */
	struct Rev : Register_array<0x20, 8, 4, 8> { }; /* product revision level */

	Inquiry_response(Byte_range_ptr const &range) : Mmio(range) { }

	bool       sbc() const { return read<Dt>() == 0x00; }
	bool removable() const { return read<Rm::Rmb>();    }

	template <typename ID>
	bool get_id(char *dst, size_t len)
	{
		if (len < ID::ITEMS+1) return false;
		for (uint32_t i = 0; i < ID::ITEMS; i++) dst[i] = read<ID>(i);
		dst[ID::ITEMS] = 0;
		return true;
	}

	void dump()
	{
		Genode::log("--- Dump INQUIRY data ---");
		Genode::log("Dt:      ", Genode::Hex(read<Dt>()));
		Genode::log("Rm::Rmb: ", read<Rm::Rmb>());
		Genode::log("Ver:     ", Genode::Hex(read<Ver>()));
		Genode::log("Rdf:     ", Genode::Hex(read<Rdf>()));
		Genode::log("Al:      ", read<Al>());
		Genode::log("Flg:     ", Genode::Hex(read<Flg>()));
	}
};


struct Scsi::Request_sense_response : Genode::Mmio<0x13>
{
	enum { LENGTH = 18 };

	struct Rc  : Register<0x0,   8>
	{
		struct V  : Bitfield<6, 1> { };  /* valid bit */
		struct Ec : Bitfield<0, 7> { };  /* error code */
	}; /* response code */
	struct Flg : Register<0x2,   8>
	{
		struct Sk : Bitfield<0, 4> { };  /* sense key */
	}; /* flags */
	struct Inf : Register<0x3, 32> { }; /* information BE */
	struct Asl : Register<0x7,  8> { }; /* additional sense length */
	struct Csi : Register<0x8, 32> { }; /* command specific information BE */
	struct Asc : Register<0xc,  8> { }; /* additional sense code */
	struct Asq : Register<0xd,  8> { }; /* additional sense code qualifier */
	struct Fru : Register<0xe,  8> { }; /* field replaceable unit code */
	struct Sks : Register<0xf, 32> { }; /* sense key specific (3 byte) */

	Request_sense_response(Byte_range_ptr const &range) : Mmio(range) { }

	void dump()
	{
		Genode::log("--- Dump REQUEST_SENSE data ---");
		Genode::log("Rc::V:   ", read<Rc::V>());
		Genode::log("Rc::Ec:  ", Genode::Hex(read<Rc::Ec>()));
		Genode::log("Flg::Sk: ", Genode::Hex(read<Flg::Sk>()));
		Genode::log("Asc:     ", Genode::Hex(read<Asc>()));
		Genode::log("Asq:     ", Genode::Hex(read<Asq>()));
	}
};


struct Scsi::Capacity_response_10 : Genode::Mmio<0x8>
{
	enum { LENGTH = 8 };

	struct Lba : Register<0x0, 32> { };
	struct Bs  : Register<0x4, 32> { };

	Capacity_response_10(Byte_range_ptr const &range) : Mmio(range) { }

	uint32_t last_block() const { return be(read<Lba>()); }
	uint32_t block_size() const { return be(read<Bs>()); }

	void dump()
	{
		Genode::log("--- Dump READ_CAPACITY_10 data ---");
		Genode::log("Lba: ", Genode::Hex(last_block()));
		Genode::log("Bs: ", Genode::Hex(block_size()));
	}
};


struct Scsi::Capacity_response_16 : Genode::Mmio<0xc>
{
	enum { LENGTH = 32 };

	struct Lba : Register<0x0, 64> { };
	struct Bs  : Register<0x8, 32> { };

	Capacity_response_16(Byte_range_ptr const &range) : Mmio(range) { }

	uint64_t last_block() const { return be(read<Lba>()); }
	uint32_t block_size() const { return be(read<Bs>()); }

	void dump()
	{
		Genode::log("--- Dump READ_CAPACITY_16 data ---");
		Genode::log("Lba: ", Genode::Hex(last_block()));
		Genode::log("Bs: ", Genode::Hex(block_size()));
	}
};


/*************************
 ** CBD 6 byte commands **
 *************************/

struct Scsi::Cmd_6 : Genode::Mmio<0x6>
{
	enum { LENGTH = 6 };
	struct Op  : Register<0x0,  8> { }; /* SCSI command */
	struct Lba : Register<0x2, 16> { }; /* logical block address */
	struct Len : Register<0x4,  8> { }; /* transfer length */
	struct Ctl : Register<0x5,  8> { }; /* controll */

	Cmd_6(Byte_range_ptr const &range) : Mmio(range) { memset((void*)base(), 0, LENGTH); }

	void dump()
	{
		Genode::log("Op:  ", Genode::Hex(read<Op>()));
		Genode::log("Lba: ", Genode::Hex(be(read<Lba>())));
		Genode::log("Len: ", read<Len>());
		Genode::log("Ctl: ", Genode::Hex(read<Ctl>()));
	}
};


struct Scsi::Test_unit_ready : Cmd_6
{
	Test_unit_ready(Byte_range_ptr const &range) : Cmd_6(range)
	{
		write<Cmd_6::Op>(Opcode::TEST_UNIT_READY);
	}
};


struct Scsi::Request_sense : Cmd_6
{
	Request_sense(Byte_range_ptr const &range) : Cmd_6(range)
	{
		write<Cmd_6::Op>(Opcode::REQUEST_SENSE);
		write<Cmd_6::Len>(Request_sense_response::LENGTH);
	}
};


struct Scsi::Inquiry : Cmd_6
{
	Inquiry(Byte_range_ptr const &range) : Cmd_6(range)
	{
		write<Cmd_6::Op>(Opcode::INQUIRY);
		write<Cmd_6::Len>(Inquiry_response::LENGTH);
	}
};


struct Scsi::Start_stop : Genode::Mmio<0x5>
{
	enum { LENGTH = 6 };
	struct Op  : Register<0x0,  8> { }; /* SCSI command */
	struct I   : Register<0x1,  8> {
		struct Immed : Bitfield<0, 1> { }; }; /* immediate */
	struct Flg : Register<0x4,  8>
	{
		struct Pwc  : Bitfield<4, 4> { }; /* power condition */
		struct Loej : Bitfield<1, 1> { }; /* load eject */
		struct St   : Bitfield<0, 1> { }; /* start */
	}; /* flags */

	Start_stop(Byte_range_ptr const &range) : Mmio(range)
	{
		memset((void*)base(), 0, LENGTH);

		write<Op>(Opcode::START_STOP);
		write<I::Immed>(1);
		write<Flg::Pwc>(0);
		write<Flg::Loej>(1);
		write<Flg::St>(1);
	}

	void dump()
	{
		Genode::log("Op:        ", Genode::Hex(read<Op>()));
		Genode::log("I::Immed:  ", read<I::Immed>());
		Genode::log("Flg::Pwc:  ", Genode::Hex(read<Flg::Pwc>()));
		Genode::log("Flg::Loej: ", read<Flg::Loej>());
		Genode::log("Flg::St:   ", read<Flg::St>());
	}
};


/**************************
 ** CBD 10 byte commands **
 **************************/

struct Scsi::Cmd_10 : Genode::Mmio<0xa>
{
	enum { LENGTH = 10 };
	struct Op  : Register<0x0,  8> { }; /* SCSI command */
	struct Lba : Register<0x2, 32> { }; /* logical block address */
	struct Len : Register<0x7, 16> { }; /* transfer length */
	struct Ctl : Register<0x9,  8> { }; /* controll */

	Cmd_10(Byte_range_ptr const &range) : Mmio(range) { memset((void*)base(), 0, LENGTH); }

	void dump()
	{
		Genode::log("Op:  ", Genode::Hex(read<Op>()));
		Genode::log("Lba: ", Genode::Hex(be(read<Lba>())));
		Genode::log("Len: ", be(read<Len>()));
		Genode::log("Ctl: ", Genode::Hex(read<Ctl>()));
	}
};


struct Scsi::Read_capacity_10 : Cmd_10
{
	Read_capacity_10(Byte_range_ptr const &range) : Cmd_10(range)
	{
		write<Cmd_10::Op>(Opcode::READ_CAPACITY_10);
	}
};


struct Scsi::Io_10 : Cmd_10
{
	Io_10(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Cmd_10(range)
	{
		write<Cmd_10::Lba>(be(lba));
		write<Cmd_10::Len>(be(len));
	}
};


struct Scsi::Read_10 : Io_10
{
	Read_10(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Io_10(range, lba, len)
	{
		write<Cmd_10::Op>(Opcode::READ_10);
	}
};


struct Scsi::Write_10 : Io_10
{
	Write_10(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Io_10(range, lba, len)
	{
		write<Cmd_10::Op>(Opcode::WRITE_10);
	}
};


/***********************************
 ** CBD 16 long LBA byte commands **
 ***********************************/

struct Scsi::Cmd_16 : Genode::Mmio<0x10>
{
	enum { LENGTH = 16 };
	struct Op  : Register<0x0,  8> { }; /* SCSI command */
	struct Lba : Register<0x2, 64> { }; /* logical block address */
	struct Len : Register<0xa, 32> { }; /* transfer length */
	struct Ctl : Register<0xf,  8> { }; /* controll */

	Cmd_16(Byte_range_ptr const &range) : Mmio(range) { memset((void*)base(), 0, LENGTH); }

	void dump()
	{
		Genode::log("Op:  ", Genode::Hex(read<Op>()));
		Genode::log("Lba: ", Genode::Hex(be(read<Lba>())));
		Genode::log("Len: ", be(read<Len>()));
		Genode::log("Ctl: ", Genode::Hex(read<Ctl>()));
	}
};


struct Scsi::Read_capacity_16 : Cmd_16
{
	Read_capacity_16(Byte_range_ptr const &range) : Cmd_16(range)
	{
		write<Cmd_16::Op>(Opcode::READ_CAPACITY_16);
	}
};


struct Scsi::Io_16 : Cmd_16
{
	Io_16(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Cmd_16(range)
	{
		write<Cmd_16::Lba>(be(lba));
		write<Cmd_16::Len>(be(len));
	}
};


struct Scsi::Read_16 : Io_16
{
	Read_16(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Io_16(range, lba, len)
	{
		write<Cmd_16::Op>(Opcode::READ_16);
	}
};


struct Scsi::Write_16 : Io_16
{
	Write_16(Byte_range_ptr const &range, uint32_t lba, uint16_t len) : Io_16(range, lba, len)
	{
		write<Cmd_16::Op>(Opcode::WRITE_16);
	}
};

#endif /* _SCSI_H_ */
