/*
 * \brief  OMAP4-specific implementation of the Block::Driver interface
 * \author Norman Feske
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_H_
#define _DRIVER_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <drivers/defs/panda.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>

/* local includes */
#include <driver_base.h>

namespace Sd_card { class Driver; }


class Sd_card::Driver : public  Driver_base,
                        private Attached_mmio
{
	private:

		enum {
			MMCHS1_MMIO_BASE = 0x4809c000,
			MMCHS1_MMIO_SIZE = 0x00001000,
		};

		enum Bus_width { BUS_WIDTH_1, BUS_WIDTH_4 };

		enum Clock_divider { CLOCK_DIV_0, CLOCK_DIV_240 };

		enum Voltage { VOLTAGE_3_0, VOLTAGE_1_8 };

		struct Sysconfig : Register<0x110, 32> { };

		struct Con : Register<0x12c, 32>
		{
			struct Init : Bitfield<1, 1>  { };
			struct Dw8  : Bitfield<5, 1>  { };
		};

		struct Cmd : Register<0x20c, 32>
		{
			struct Bce   : Bitfield<1, 1>  { };
			struct Acen  : Bitfield<2, 1>  { };
			struct Msbs  : Bitfield<5, 1>  { };
			struct Index : Bitfield<24, 6> { };
			struct Dp    : Bitfield<21, 1> { };

			struct Rsp_type : Bitfield<16, 2>
			{
				enum Response { RESPONSE_NONE             = 0,
				                RESPONSE_136_BIT          = 1,
				                RESPONSE_48_BIT           = 2,
				                RESPONSE_48_BIT_WITH_BUSY = 3 };
			};

			struct Ddir : Bitfield<4, 1>
			{
				enum { WRITE = 0, READ = 1 };
			};
		};

		struct Blk : Register<0x204, 32>
		{
			struct Blen : Bitfield<0, 12> { };
			struct Nblk : Bitfield<16, 16> { };
		};

		struct Arg   : Register<0x208, 32> { };
		struct Rsp10 : Register<0x210, 32> { };
		struct Rsp32 : Register<0x214, 32> { };
		struct Rsp54 : Register<0x218, 32> { };
		struct Rsp76 : Register<0x21c, 32> { };
		struct Data  : Register<0x220, 32> { };

		struct Pstate : Register<0x224, 32>
		{
			struct Cmdi : Bitfield<0, 1>  { };
			struct Bwe  : Bitfield<10, 1> { };
			struct Bre  : Bitfield<11, 1> { };
		};

		struct Hctl : Register<0x228, 32>
		{
			struct Sdbp : Bitfield<8, 1>
			{
				enum { POWER_OFF = 0, POWER_ON = 1 };
			};

			struct Sdvs : Bitfield<9, 3>
			{
				enum Voltage { VOLTAGE_1_8 = 5,
				               VOLTAGE_3_0 = 6,
				               VOLTAGE_3_3 = 7 };
			};

			struct Dtw : Bitfield<1, 1>
			{
				enum { ONE_BIT = 0, FOUR_BITS = 1 };
			};
		};

		struct Sysctl : Register<0x22c, 32>
		{
			struct Ice  : Bitfield<0, 1>  { };
			struct Ics  : Bitfield<1, 1>  { };
			struct Ce   : Bitfield<2, 1>  { };
			struct Clkd : Bitfield<6, 10> { };
			struct Src  : Bitfield<25, 1> { };

			struct Dto : Bitfield<16, 4>
			{
				enum { TCF_2_POW_27 = 0xe };
			};
		};

		struct Stat : Register<0x230, 32>
		{
			struct Tc   : Bitfield<1, 1>  { };
			struct Cc   : Bitfield<0, 1>  { };
			struct Erri : Bitfield<15, 1> { };
			struct Cto  : Bitfield<16, 1> { };
		};

		struct Ie : Register<0x234, 32>
		{
			struct Tc_enable  : Bitfield<1, 1>  { };
			struct Cto_enable : Bitfield<16, 1> { };
		};

		struct Ise : Register<0x238, 32>
		{
			struct Tc_sigen  : Bitfield<1, 1>  { };
			struct Cto_sigen : Bitfield<16, 1> { };
		};

		struct Capa : Register<0x240, 32>
		{
			struct Vs30 : Bitfield<25, 1> { };
			struct Vs18 : Bitfield<26, 1> { };
		};

		struct Block_transfer
		{
			Block::Packet_descriptor packet;
			bool                     pending = false;
		};

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			Timer_delayer(Genode::Env &env) : Timer::Connection(env) { }

			void usleep(unsigned us) { Timer::Connection::usleep(us); }
		};

		Env                    &_env;
		Block_transfer          _block_transfer;
		Timer_delayer           _delayer     { _env };
		Signal_handler<Driver>  _irq_handler { _env.ep(), *this, &Driver::_handle_irq };
		Irq_connection          _irq         { _env, Panda::HSMMC_IRQ };
		Card_info               _card_info   { _init() };

		Card_info _init();
		bool      _wait_for_bre();
		bool      _wait_for_bwe();
		void      _handle_irq();
		bool      _reset_cmd_line();
		void      _disable_irq();
		void      _bus_width(Bus_width bus_width);
		bool      _sd_bus_power_on();
		bool      _set_and_enable_clock(enum Clock_divider divider);
		void      _set_bus_power(Voltage voltage);
		bool      _init_stream();

		void _stop_clock() { Mmio::write<Sysctl::Ce>(0); }


		/*********************
		 ** Host_controller **
		 *********************/

		bool _issue_command(Command_base const &command) override;
		Cid _read_cid() override;
		Csd _read_csd() override;

		Card_info card_info() const override { return _card_info; }

		unsigned _read_rca() override {
			return Send_relative_addr::Response::Rca::get(Mmio::read<Rsp10>()); }

	public:

		Driver(Env &env);


		/*******************
		 ** Block::Driver **
		 *******************/

		void read(Block::sector_t           block_number,
		          size_t                    block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &pkt) override;

		void write(Block::sector_t           block_number,
		           size_t                    block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &pkt) override;
};

#endif /* _DRIVER_H_ */
