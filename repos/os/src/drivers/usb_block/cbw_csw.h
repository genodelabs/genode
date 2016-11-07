/*
 * \brief  USB Command Block and Status Wrapper
 * \author Josef Soentgen
 * \date   2016-02-08
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CBW_CSW_H_
#define _CBW_CSW_H_

#include <scsi.h>

namespace Usb {
	struct Cbw;
	struct Csw;
}


using Genode::uint8_t;
using Genode::uint32_t;
using Genode::uint64_t;
using Genode::size_t;
using Genode::addr_t;


/*****************************************************
 ** USB Command Block/Status Wrapper implementation **
 *****************************************************/

struct Usb::Cbw : Genode::Mmio
{
	enum { LENGTH = 31 };

	enum { SIG = 0x43425355U };
	struct Sig : Register<0x0, 32> { }; /* signature */
	struct Tag : Register<0x4, 32> { }; /* transaction unique identifier */
	struct Dtl : Register<0x8, 32> { }; /* data transfer length */
	struct Flg : Register<0xc,  8> { }; /* flags */
	struct Lun : Register<0xd,  8> { }; /* logical unit number */
	struct Cbl : Register<0xe,  8> { }; /* command buffer length */

	Cbw(addr_t addr, uint32_t t, uint32_t d,
	    uint8_t f, uint8_t l, uint8_t len) : Mmio(addr)
	{
		write<Sig>(SIG);
		write<Tag>(t);
		write<Dtl>(d);
		write<Flg>(f);
		write<Lun>(l);
		write<Cbl>(len);
	}

	void dump()
	{
		Genode::log("Sig: ", Genode::Hex(read<Sig>()));
		Genode::log("Tag: ", read<Tag>());
		Genode::log("Dtl: ", read<Dtl>());
		Genode::log("Flg: ", Genode::Hex(read<Flg>()));
		Genode::log("Lun: ", read<Lun>());
		Genode::log("Cbl: ", read<Cbl>());
	}
};


struct Test_unit_ready : Usb::Cbw,
                         Scsi::Test_unit_ready
{
	Test_unit_ready(addr_t addr, uint32_t tag, uint8_t lun)
	:
		Cbw(addr, tag, 0, Usb::ENDPOINT_IN, lun,
		    Scsi::Test_unit_ready::LENGTH),
		Scsi::Test_unit_ready(addr+15)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump TEST_UNIT_READY command --");
		Cbw::dump();
		Scsi::Cmd_6::dump();
	}
};


struct Request_sense : Usb::Cbw, Scsi::Request_sense
{
	Request_sense(addr_t addr, uint32_t tag, uint8_t lun)
	:
		Cbw(addr, tag, Scsi::Request_sense_response::LENGTH,
		    Usb::ENDPOINT_IN, lun, Scsi::Request_sense::LENGTH),
		Scsi::Request_sense(addr+15)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump REQUEST_SENSE command --");
		Cbw::dump();
		Scsi::Cmd_6::dump();
	}
};


struct Inquiry : Usb::Cbw, Scsi::Inquiry
{
	Inquiry(addr_t addr, uint32_t tag, uint8_t lun)
	:
		Cbw(addr, tag, Scsi::Inquiry_response::LENGTH,
		    Usb::ENDPOINT_IN, lun, Scsi::Inquiry::LENGTH),
		Scsi::Inquiry(addr+15)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump INQUIRY command --");
		Cbw::dump();
		Scsi::Cmd_6::dump();
	}
};


struct Read_capacity_10 : Usb::Cbw, Scsi::Read_capacity_10
{
	Read_capacity_10(addr_t addr, uint32_t tag, uint8_t lun)
	:
		Cbw(addr, tag, Scsi::Capacity_response_10::LENGTH,
		    Usb::ENDPOINT_IN, lun, Scsi::Read_capacity_10::LENGTH),
		Scsi::Read_capacity_10(addr+15)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump READ_CAPACITY_10 command --");
		Cbw::dump();
		Scsi::Cmd_10::dump();
	}
};


struct Read_10 : Usb::Cbw, Scsi::Read_10
{
	Read_10(addr_t addr, uint32_t tag, uint8_t lun,
	        uint32_t lba, uint32_t len, uint32_t block_size)
	:
		Cbw(addr, tag, len * block_size,
		    Usb::ENDPOINT_IN, lun, Scsi::Read_10::LENGTH),
		Scsi::Read_10(addr+15, lba, len)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump READ_10 command --");
		Cbw::dump();
		Scsi::Cmd_10::dump();
	}
};


struct Write_10 : Usb::Cbw, Scsi::Write_10
{
	Write_10(addr_t addr, uint32_t tag, uint8_t lun,
             uint32_t lba, uint32_t len, uint32_t block_size)
	:
		Cbw(addr, tag, len * block_size,
		    Usb::ENDPOINT_OUT, lun, Scsi::Write_10::LENGTH),
		Scsi::Write_10(addr+15, lba, len)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump WRITE_10 command --");
		Cbw::dump();
		Scsi::Cmd_10::dump();
	}
};


struct Read_capacity_16 : Usb::Cbw, Scsi::Read_capacity_16
{
	Read_capacity_16(addr_t addr, uint32_t tag, uint8_t lun)
	:
		Cbw(addr, tag, Scsi::Capacity_response_16::LENGTH,
		    Usb::ENDPOINT_IN, lun, Scsi::Read_capacity_16::LENGTH),
		Scsi::Read_capacity_16(addr+15)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump READ_CAPACITY_16 command --");
		Cbw::dump();
		Scsi::Cmd_16::dump();
	}
};


struct Read_16 : Usb::Cbw, Scsi::Read_16
{
	Read_16(addr_t addr, uint32_t tag, uint8_t lun,
	        uint32_t lba, uint32_t len, uint32_t block_size)
	:
		Cbw(addr, tag, len * block_size,
		    Usb::ENDPOINT_IN, lun, Scsi::Read_16::LENGTH),
		Scsi::Read_16(addr+15, lba, len)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump READ_16 command --");
		Cbw::dump();
		Scsi::Cmd_16::dump();
	}
};


struct Write_16 : Usb::Cbw, Scsi::Write_16
{
	Write_16(addr_t addr, uint32_t tag, uint8_t lun,
             uint32_t lba, uint32_t len, uint32_t block_size)
	:
		Cbw(addr, tag, len * block_size,
		    Usb::ENDPOINT_OUT, lun, Scsi::Write_16::LENGTH),
		Scsi::Write_16(addr+15, lba, len)
	{ if (verbose_scsi) dump(); }

	void dump()
	{
		Genode::log("--- Dump WRITE_16 command --");
		Cbw::dump();
		Scsi::Cmd_16::dump();
	}
};


struct Usb::Csw : Genode::Mmio
{
	enum { LENGTH = 13 };

	enum { SIG = 0x53425355U };
	struct Sig : Register<0x0, 32> { }; /* signature */
	struct Tag : Register<0x4, 32> { }; /* transaction identifier */
	struct Dr  : Register<0x8, 32> { }; /* data residue */
	enum { PASSED = 0, FAILED = 1, PHASE_ERROR = 2 };
	struct Sts : Register<0xc, 8> { }; /* status */

	Csw(addr_t addr) : Mmio(addr) { }

	uint32_t sig() const { return read<Sig>(); }
	uint32_t tag() const { return read<Tag>(); }
	uint32_t dr()  const { return read<Dr>();  }
	uint32_t sts() const { return read<Sts>(); }
};

#endif /* _CBW_CSW_H_ */
