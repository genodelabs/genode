/*
 * \brief  Exynos5-specific implementation of the Block::Driver interface
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2013-03-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <timer_session/connection.h>
#include <drivers/defs/exynos5.h>
#include <regulator_session/connection.h>
#include <irq_session/connection.h>
#include <base/attached_ram_dataspace.h>

/* local includes */
#include <driver_base.h>

namespace Sd_card { class Driver; }


class Sd_card::Driver : public  Driver_base,
                        private Attached_mmio
{
	private:

		/*
		 * Noncopyable
		 */
		Driver(Driver const &);
		Driver &operator = (Driver const &);

		enum {
			HOST_FREQ              = 52000000,
			CLK_FREQ               = 400000000,
			CLK_DIV_52Mhz          = 4,
			CLK_DIV_400Khz         = 0xff,
			MSH_BASE               = 0x12200000,
			MSH_SIZE               = 0x10000,
			IDMAC_DESC_MAX_ENTRIES = 1024
		};

		enum Bus_width {
			BUS_WIDTH_1 = 0,
			BUS_WIDTH_4 = 1,
			BUS_WIDTH_8 = 1 << 16,
		};

		template <off_t OFFSET, bool STRICT_WRITE = false>
		struct Register : Mmio::Register<OFFSET, 32, STRICT_WRITE> { };

		struct Ctrl : Register<0x0>
		{
			struct Reset             : Bitfield<0, 3> { };
			struct Global_interrupt  : Bitfield<4, 1> { };
			struct Dma_enable        : Bitfield<5, 1> { };
			struct Use_internal_dmac : Bitfield<25, 1> { };
		};

		struct Pwren   : Register<0x4> { };
		struct Clkdiv  : Register<0x8> { };
		struct Clkena  : Register<0x10> { };
		struct Tmout   : Register<0x14> { };
		struct Ctype   : Register<0x18, true> { };
		struct Blksize : Register<0x1c> { };
		struct Bytcnt  : Register<0x20> { };
		struct Intmask : Register<0x24> { };
		struct Cmdarg  : Register<0x28> { };

		struct Cmd : Register<0x2c>
		{
			struct Index                       : Bitfield<0,  6> { };
			struct Rsp_type                    : Bitfield<6,  3>
			{
				enum Response { RESPONSE_NONE             = 0,
				                RESPONSE_48_BIT           = 1,
				                RESPONSE_48_BIT_WITH_BUSY = 5,
				                RESPONSE_136_BIT          = 7,
				};
			};
			struct Data_expected               : Bitfield<9,  1> { };
			struct Write                       : Bitfield<10, 1> { };
			struct Wait_prvdata_complete       : Bitfield<13, 1> { };
			struct Init_sequence               : Bitfield<15, 1> { };
			struct Update_clock_registers_only : Bitfield<21, 1> { };
			struct Use_hold_reg                : Bitfield<29, 1> { };
			struct Start_cmd                   : Bitfield<31, 1> { };
		};

		struct Rsp0 : Register<0x30> { };
		struct Rsp1 : Register<0x34> { };
		struct Rsp2 : Register<0x38> { };
		struct Rsp3 : Register<0x3c> { };

		struct Mintsts : Register<0x40> { };
		struct Rintsts : Register<0x44, true>
		{
			struct Response_error     : Bitfield<1, 1> { };
			struct Data_transfer_over : Bitfield<3, 1> { };
			struct Command_done       : Bitfield<2, 1> { };
			struct Data_crc_error     : Bitfield<7, 1> { };
			struct Response_timeout   : Bitfield<8, 1> { };
			struct Data_read_timeout  : Bitfield<9, 1> { };
		};

		struct Status : Register<0x48>
		{
			struct Data_busy : Bitfield<9, 1> { };
		};

		struct Fifoth : Register<0x4c> { };

		struct Bmod : Register<0x80, true>
		{
			struct Fixed_burst : Bitfield<1, 1> { };
			struct Idmac_enable : Bitfield<7, 1> { };
		};

		struct Pldmnd       : Register<0x84> { };
		struct Idsts        : Register<0x8c> { };
		struct Idinten      : Register<0x90, true> { };
		struct Dbaddr       : Register<0x88> { };
		struct Clksel       : Register<0x9c> { };
		struct Emmc_ddr_req : Register<0x10c, true> { };

		struct Idmac_desc
		{
			enum Flags {
				NONE = 0,
				DIC  = 1 << 1,
				LD   = 1 << 2,
				FS   = 1 << 3,
				CH   = 1 << 4,
				ER   = 1 << 5,
				OWN  = 1 << 31,
			};

			unsigned  flags;
			unsigned  bytes;
			unsigned  addr;
			unsigned  next;

			size_t set(size_t block_count,
			           size_t block_size,
			           addr_t phys_addr,
			           Flags  flag);
		};

		struct Clock_regulator
		{
			Regulator::Connection regulator;

			Clock_regulator(Env &env) : regulator(env, Regulator::CLK_MMC0) {
				regulator.state(true); }
		};

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			Timer_delayer(Genode::Env &env) : Timer::Connection(env) { }

			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		};

		struct Block_transfer
		{
			Block::Packet_descriptor packet { };
			bool                     pending = false;
		};

		Env                    &_env;
		Timer_delayer           _delayer          { _env };
		Block_transfer          _block_transfer   { };
		Clock_regulator         _clock_regulator  { _env };
		Signal_handler<Driver>  _irq_handler      { _env.ep(), *this, &Driver::_handle_irq };
		Irq_connection          _irq              { _env,  Exynos5::SDMMC0_IRQ };
		Attached_ram_dataspace  _idmac_desc_ds    { _env.ram(), _env.rm(),
		                                            IDMAC_DESC_MAX_ENTRIES * sizeof(Idmac_desc),
		                                            UNCACHED };
		Idmac_desc *const       _idmac_desc       { _idmac_desc_ds.local_addr<Idmac_desc>() };
		addr_t      const       _idmac_desc_phys  { Dataspace_client(_idmac_desc_ds.cap())
		                                            .phys_addr() };
		Card_info               _card_info        { _init() };

		bool      _reset();
		void      _reset_fifo();
		void      _disable_irq();
		bool      _update_clock_registers();
		bool      _setup_bus(unsigned clock_div);
		void      _handle_irq();
		Card_info _init();

		bool _setup_idmac_descriptor_table(size_t block_count,
		                                   addr_t phys_addr);


		/*********************
		 ** Host_controller **
		 *********************/

		bool     _issue_command(Command_base const &command) override;
		Cid      _read_cid() override ;
		Csd      _read_csd() override ;
		size_t   _read_ext_csd() override;
		unsigned _read_rca() override { return 0; }

		Card_info card_info() const override { return _card_info; }

	public:

		using Block::Driver::read;
		using Block::Driver::write;

		Driver(Env &env);


		/*******************
		 ** Block::Driver **
		 *******************/

		void read_dma(Block::sector_t           block_number,
		              size_t                    block_count,
		              addr_t                    buf_phys,
		              Block::Packet_descriptor &pkt) override;

		void write_dma(Block::sector_t           block_number,
		               size_t                    block_count,
		               addr_t                    buf_phys,
		               Block::Packet_descriptor &pkt) override;

		bool dma_enabled() override { return true; }

		Ram_dataspace_capability alloc_dma_buffer(size_t size) override {
			return _env.ram().alloc(size, UNCACHED); }
};

#endif /* _DRIVER_H_ */
