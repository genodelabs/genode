/*
 * \brief  SD card protocol definitions
 * \author Norman Feske
 * \date   2012-07-06
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

		struct Device_size_lo : Bitfield<48 - BIT_BASE, 16> { };
	};

	struct Csd2 : Register<32>
	{
		enum { BIT_BASE = 2*sizeof(access_t)*8 };

		struct Device_size_hi : Bitfield<64 - BIT_BASE, 6> { };
	};

	struct Csd3 : Register<32>
	{
		enum { BIT_BASE = 3*sizeof(access_t)*8 };

		struct Version : Bitfield<126 - BIT_BASE, 2>
		{
			enum { HIGH_CAPACITY = 1 };
		};
	};

	struct Csd
	{
		Csd0::access_t csd0;
		Csd1::access_t csd1;
		Csd2::access_t csd2;
		Csd3::access_t csd3;
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

	struct Set_block_count : Command<23, RESPONSE_48_BIT>
	{
		Set_block_count(size_t count)
		{
			arg = count;
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
				     cid.raw_0, cid.raw_1, cid.raw_2, cid.raw_3);

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

				if (Csd3::Version::get(csd.csd3) != Csd3::Version::HIGH_CAPACITY) {
					PERR("Could not detect high-capacity card");
					throw Detection_failed();
				}

				size_t const device_size = ((Csd2::Device_size_hi::get(csd.csd2) << 16)
				                           | Csd1::Device_size_lo::get(csd.csd1)) + 1;

				if (!issue_command(Select_card(rca))) {
					PERR("Select_card failed");
					throw Detection_failed();
				}

				return Card_info(rca, device_size / 2);
			}
	};
}

#endif /* _SD_CARD_H_ */
