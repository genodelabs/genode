/*
 * \brief  SD card protocol definitions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2012-07-06
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

/* Genode includes */
#include <util/register.h>

namespace Sd_card {

	using namespace Genode;

	/**
	 * Returned by 'Sd_send_op_cond'
	 */
	struct Ocr : Register<32>
	{
		struct Busy : Bitfield<31, 1> { };
	};

	struct Cid {
		uint32_t raw_0;
		uint32_t raw_1;
		uint32_t raw_2;
		uint32_t raw_3;
	};

	struct Csd0 : Register<32>
	{
	};

	struct Csd1 : Register<32>
	{
		enum { BIT_BASE = 1*sizeof(access_t)*8 };

		struct V2_device_size_lo : Bitfield<48 - BIT_BASE, 16> { };

		struct V1_c_size_lo   : Bitfield<62 - BIT_BASE, 2> { };
		struct V1_c_size_mult : Bitfield<47 - BIT_BASE, 3> { };
	};

	struct Csd2 : Register<32>
	{
		enum { BIT_BASE = 2*sizeof(access_t)*8 };

		struct V2_device_size_hi : Bitfield<64 - BIT_BASE, 6> { };

		struct V1_read_bl_len : Bitfield<80 - BIT_BASE, 4>  { };
		struct V1_c_size_hi   : Bitfield<64 - BIT_BASE, 10> { };
	};

	struct Csd3 : Register<32>
	{
		enum { BIT_BASE = 3*sizeof(access_t)*8 };

		struct Version : Bitfield<126 - BIT_BASE, 2>
		{
			enum { STANDARD_CAPACITY = 0, HIGH_CAPACITY = 1, EXT_CSD = 3 };
		};

		struct Mmc_spec_vers : Bitfield<122 - BIT_BASE, 4> { };
	};

	struct Csd
	{
		Csd0::access_t csd0;
		Csd1::access_t csd1;
		Csd2::access_t csd2;
		Csd3::access_t csd3;
	};


	struct Ext_csd : Mmio
	{
		Ext_csd(addr_t base) : Mmio(base) { }

		struct Revision     : Register<0xc0, 8>  { };
		struct Sector_count : Register<0xd4, 32> { };
	};

	struct Arg : Register<32> { };

	enum Response { RESPONSE_NONE,
	                RESPONSE_136_BIT,
	                RESPONSE_48_BIT,
	                RESPONSE_48_BIT_WITH_BUSY };

	enum Transfer { TRANSFER_NONE, TRANSFER_READ, TRANSFER_WRITE };

	struct Command_base
	{
		unsigned      index;    /* command opcode */
		Arg::access_t arg;      /* argument */
		Response      rsp_type; /* response type */
		Transfer      transfer; /* data transfer type */

		Command_base(unsigned op, Response rsp_type, Transfer transfer)
		:
			index(op), arg(0), rsp_type(rsp_type), transfer(transfer)
		{ }
	};

	template <unsigned _INDEX, Response RSP_TYPE, Transfer TRANSFER = TRANSFER_NONE>
	struct Command : Command_base
	{
		enum { INDEX = _INDEX };
		Command() : Command_base(_INDEX, RSP_TYPE, TRANSFER) { }
	};

	template <unsigned INDEX, Response RSP_TYPE, Transfer TRANSFER = TRANSFER_NONE>
	struct Prefixed_command : private Command_base
	{
		Prefixed_command() : Command_base(INDEX, RSP_TYPE, TRANSFER) { }

		using Command_base::arg;

		/**
		 * Used by ACMD overload of 'issue_command()'
		 */
		Command_base const &base() const { return *this; }
	};

	struct Go_idle_state : Command<0, RESPONSE_NONE> { };

	struct All_send_cid : Command<2, RESPONSE_136_BIT> { };

	struct Send_relative_addr : Command<3, RESPONSE_48_BIT>
	{
		struct Response : Sd_card::Arg
		{
			struct Rca : Bitfield<16, 16> { };
		};

		Send_relative_addr(unsigned rca = 0)
		{
			Response::Rca::set(arg, rca);
		}
	};

	struct Select_card : Command<7, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			struct Rca : Bitfield<16, 16> { };
		};

		Select_card(unsigned rca)
		{
			Arg::Rca::set(arg, rca);
		}
	};

	struct Send_if_cond : Command<8, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			struct Check_pattern  : Bitfield<0, 8> { };
			struct Supply_voltage : Bitfield<8, 4> { };
		};

		Send_if_cond()
		{
			Arg::Check_pattern::set(arg, 0xaa);
			Arg::Supply_voltage::set(arg, 1);
		}
	};

	struct Send_csd : Command<9, RESPONSE_136_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			struct Rca : Bitfield<16, 16> { };
		};

		Send_csd(unsigned rca)
		{
			Arg::Rca::set(arg, rca);
		}
	};

	struct Mmc_send_ext_csd : Command<8, RESPONSE_48_BIT_WITH_BUSY, TRANSFER_READ>
	{ };

	struct Set_block_count : Command<23, RESPONSE_48_BIT>
	{
		Set_block_count(size_t count)
		{
			arg = count;
		};
	};

	struct Set_blocklen : Command<16, RESPONSE_48_BIT>
	{
		Set_blocklen(size_t blocklen)
		{
			arg = blocklen;
		};
	};

	struct Read_multiple_block : Command<18, RESPONSE_48_BIT, TRANSFER_READ>
	{
		Read_multiple_block(unsigned long addr)
		{
			arg = addr;
		}
	};

	struct Write_multiple_block : Command<25, RESPONSE_48_BIT, TRANSFER_WRITE>
	{
		Write_multiple_block(unsigned long addr)
		{
			arg = addr;
		}
	};

	struct Set_bus_width : Prefixed_command<6, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			struct Bus_width : Bitfield<0, 2>
			{
				enum Width { ONE_BIT = 0, FOUR_BITS = 2 };
			};
		};

		Set_bus_width(Arg::Bus_width::Width width)
		{
			Arg::Bus_width::set(arg, width);
		}
	};

	struct Mmc_switch : Command<6, RESPONSE_48_BIT>
	{
		enum { SWITCH_MODE_WRITE_BYTE = 0x3 };

		struct Arg : Sd_card::Arg
		{
			struct Value : Bitfield<8,  8> { };
			struct Index : Bitfield<16, 8> { };
			struct Mode  : Bitfield<24, 8> { };
		};

		Mmc_switch(unsigned index, unsigned val)
		{
			Arg::Mode::set(arg,  SWITCH_MODE_WRITE_BYTE);
			Arg::Index::set(arg, index);
			Arg::Value::set(arg, val);
		}
	};

	struct Sd_send_op_cond : Prefixed_command<41, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			/**
			 * Operating condition register
			 */
			struct Ocr : Bitfield<0, 24> { };

			/**
			 * Host capacity support
			 */
			struct Hcs : Bitfield<30, 1> { };
		};

		Sd_send_op_cond(unsigned ocr, bool hcs)
		{
			Arg::Ocr::set(arg, ocr);
			Arg::Hcs::set(arg, hcs);
		}
	};

	struct Mmc_send_op_cond : Command<1, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			/**
			 * Operating condition register
			 */
			struct Ocr : Bitfield<0, 24> { };

			/**
			 * Host capacity support
			 */
			struct Hcs : Bitfield<30, 1> { };
		};

		Mmc_send_op_cond(unsigned ocr, bool hcs)
		{
			Arg::Ocr::set(arg, ocr);
			Arg::Hcs::set(arg, hcs);
		}
	};

	struct Stop_transmission : Command<12, RESPONSE_48_BIT> { };

	struct Acmd_prefix : Command<55, RESPONSE_48_BIT>
	{
		struct Arg : Sd_card::Arg
		{
			struct Rca : Bitfield<16, 16> { };
		};

		Acmd_prefix(unsigned rca)
		{
			Arg::Rca::set(arg, rca);
		}
	};

	class Card_info
	{
		private:

			unsigned _rca;
			size_t   _capacity_mb;

		public:

			Card_info(unsigned rca, size_t capacity_mb)
			: _rca(rca), _capacity_mb(capacity_mb)
			{ }

			/**
			 * Return capacity in megabytes
			 */
			size_t capacity_mb() const { return _capacity_mb; }

			/**
			 * Return relative card address
			 */
			unsigned rca() const { return _rca; }
	};


	/**
	 * SD card host controller
	 */
	class Host_controller
	{
		public:

			/**
			 * Exception type
			 */
			struct Detection_failed { };

		protected:

			virtual bool _issue_command(Command_base const &command) = 0;

			virtual Cid _read_cid() = 0;

			virtual Csd _read_csd() = 0;

			virtual unsigned _read_rca() = 0;

			virtual size_t _read_ext_csd() { return 0; }

		public:

			virtual Card_info card_info() const = 0;

			bool issue_command(Command_base const &command)
			{
				return _issue_command(command);
			}

			/**
			 * Issue application-specific command
			 *
			 * This overload is selected if the supplied command type has
			 * 'Prefixed_command' as its base class. In this case, we need to
			 * issue a CMD55 as command prefix followed by the actual command.
			 *
			 * \param prefix_rca  argument to CMD55 prefix command
			 */
			template <unsigned INDEX, Response RSP_TYPE, Transfer TRANSFER>
			bool issue_command(Prefixed_command<INDEX, RSP_TYPE, TRANSFER> const &command,
			                   unsigned prefix_rca = 0)
			{
				/* send CMD55 prefix */
				if (!_issue_command(Acmd_prefix(prefix_rca))) {
					PERR("prefix command timed out");
					return false;
				}

				/* send actual command */
				return _issue_command(command.base());
			}

		private:

			/**
			 * Extract capacity information from CSD register
			 *
			 * \throw  Detection_failed
			 * \return capacity in 512-byte blocks
			 */
			size_t _sd_card_device_size(Csd const csd)
			{
				/*
				 * The capacity is reported via the CSD register. There are
				 * two versions of this register. Standard-capacity cards
				 * use version 1 whereas high-capacity cards use version 2.
				 */

				if (Csd3::Version::get(csd.csd3) == Csd3::Version::STANDARD_CAPACITY) {

					/*
					 * Calculation of the capacity according to the
					 * "Physical Layer Simplified Specification Version 4.10",
					 * Section 5.3.2.
					 */
					size_t const read_bl_len = Csd2::V1_read_bl_len::get(csd.csd2);
					size_t const c_size      = (Csd2::V1_c_size_hi::get(csd.csd2) << 2)
					                         |  Csd1::V1_c_size_lo::get(csd.csd1);
					size_t const c_size_mult = Csd1::V1_c_size_mult::get(csd.csd1);
					size_t const mult        = 1 << (c_size_mult + 2);
					size_t const block_len   = 1 << read_bl_len;
					size_t const capacity    = (c_size + 1)*mult*block_len;

					return capacity;
				}

				if (Csd3::Version::get(csd.csd3) == Csd3::Version::HIGH_CAPACITY)
					return ((Csd2::V2_device_size_hi::get(csd.csd2) << 16)
				           | Csd1::V2_device_size_lo::get(csd.csd1)) + 1;

				PERR("Could not detect SD-card capacity");
				throw Detection_failed();
			}

		protected:

			/**
			 * Perform SD card detection sequence
			 *
			 * \throw Detection_failed
			 */
			Card_info _detect()
			{
				if (!issue_command(All_send_cid())) {
					PWRN("All_send_cid command failed");
					throw Detection_failed();
				}

				Cid const cid = _read_cid();
				PLOG("CID: 0x%08x 0x%08x 0x%08x 0x%08x",
				     cid.raw_3, cid.raw_2, cid.raw_1, cid.raw_0);

				if (!issue_command(Send_relative_addr())) {
					PERR("Send_relative_addr timed out");
					throw Detection_failed();
				}

				unsigned const rca = _read_rca();
				PLOG("RCA: 0x%04x", rca);

				if (!issue_command(Send_csd(rca))) {
					PERR("Send_csd failed");
					throw Detection_failed();
				}

				Csd const csd = _read_csd();

				if (!issue_command(Select_card(rca))) {
					PERR("Select_card failed");
					throw Detection_failed();
				}

				return Card_info(rca, _sd_card_device_size(csd) / 2);
			}

			Card_info _detect_mmc()
			{
				if (!issue_command(All_send_cid())) {
					PWRN("All_send_cid command failed");
					throw Detection_failed();
				}

				unsigned const rca = 1;

				if (!issue_command(Send_relative_addr(rca))) {
					PERR("Send_relative_addr timed out");
					throw Detection_failed();
				}

				if (!issue_command(Send_csd(rca))) {
					PERR("Send_csd failed");
					throw Detection_failed();
				}

				Csd const csd = _read_csd();

				if (Csd3::Version::get(csd.csd3) != Csd3::Version::EXT_CSD) {
					PERR("Csd version is not extented CSD");
					throw Detection_failed();
				}

				if (Csd3::Mmc_spec_vers::get(csd.csd3) < 4) {
					PERR("Csd specific version is less than 4");
					throw Detection_failed();
				}

				if (!issue_command(Select_card(rca))) {
					PERR("Select_card failed");
					throw Detection_failed();
				}

				size_t device_size;
				if(!(device_size = _read_ext_csd())) {
					PERR("Could not read extented CSD");
					throw Detection_failed();
				}

				return Card_info(rca, device_size);
			}
	};
}

#endif /* _SD_CARD_H_ */
