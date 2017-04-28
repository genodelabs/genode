/*
 * \brief  Image Processing Unit registers
 * \author Nikolay Golikov  <nik@ksyslabs.org>
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2012-11-10
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__IMX53__IPU_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__IMX53__IPU_H_

/* Genode includes */
#include <util/mmio.h>
#include <util/string.h>
#include <base/attached_io_mem_dataspace.h>


class Ipu : Genode::Mmio
{
	private:

		enum { REGS_OFF = 0x6000000 };

		struct Conf            : Register<0x0,   32> { };

		template<unsigned NR>
		struct Int_ctrl        : Register<0x3c+(NR*4), 32> { };

		struct Srm_pri2 : Register<0xa4,  32>
		{
				struct Dp_m_srm : Bitfield<3,2> { enum { UPDATE_NOW = 1 }; };
		};
		struct Disp_gen             : Register<0xc4,  32> { };
		struct Mem_rst              : Register<0xdc,  32> { };
		struct Pm                   : Register<0xe0,  32> { };
		struct Gpr                  : Register<0xe4,  32> { };
		struct Ch_db_mode_sel0      : Register<0x150, 32> { };
		struct Alt_ch_trb_mode_sel0 : Register<0x178, 32> { };
		struct Cur_buf_0            : Register<0x23c, 32> { };
		struct Triple_cur_buf_1     : Register<0x25c, 32> { };
		struct Ch_buf0_rdy0         : Register<0x268, 32> { };
		struct Ch_buf1_rdy0         : Register<0x270, 32> { };


		/**************************************
		 **  Image DMA controller registers  **
		 **************************************/

		enum Idmac_channels {
			CHAN_DP_PRIMARY_MAIN = 23,
			CHAN_DP_PRIMARY_AUXI = 27,
			CHAN_DC_SYNC_FLOW    = 28
		};

		struct Idmac_ch_en : Register_array<0x8004, 32, 32, 1>
		{
				struct Ch : Bitfield<0, 1> { };
		};

		struct Idmac_ch_pri_1  : Register<0x8014, 32> { };

		struct Idmac_wm_en : Register_array<0x801c, 32, 32, 1>
		{
				struct Ch : Bitfield<0, 1> { };
		};

		struct Idmac_ch_lock_en_1 : Register<0x8024, 32> { };


		/***********************************
		 **  Display processor registers  **
		 ***********************************/

		struct Dp_com_conf     : Register<0x1040000, 32> { };
		struct Dp_fg_pos_sync  : Register<0x1040008, 32> { };
		struct Gr_wnd_ctl_sync : Register<0x1040004, 32> { };


		/***********************************
		 **  Display interface registers  **
		 ***********************************/

		template <Genode::off_t OFF>
		struct Di
		{
			struct General        : Register<OFF+0x0,   32> { };
			struct Bs_clkgen0     : Register<OFF+0x4,   32> { };
			struct Bs_clkgen1     : Register<OFF+0x8,   32> { };
			template <unsigned NR>
			struct Sync_wave_gen0 : Register<0xc+OFF+(NR*4),  32> { };
			template <unsigned NR>
			struct Sync_wave_gen1 : Register<0x30+OFF+(NR*4), 32> { };

				struct Sync_as_gen    : Register<OFF+0x54,  32> { };

			template <unsigned NR>
			struct Dw_gen         : Register<0x58 + OFF + (NR*4), 32> { };
			template <unsigned NR>
			struct Dw_set3        : Register<0x118 + OFF + (NR*4), 32> { };
			template <unsigned NR>
			struct Step_repeat    : Register<0x148 + OFF + (NR*4), 32> { };

			struct Polarity       : Register<OFF+0x164, 32> { };
			struct Scr_conf       : Register<OFF+0x170, 32> { };
		};

		typedef Di<0x40000> Di0;
		typedef Di<0x48000> Di1;


		/************************************
		 **  Display controller registers  **
		 ************************************/

		struct Dc_wr_ch_conf_5  : Register<0x5805c, 32> { };
		struct Dc_wr_ch_addr_5  : Register<0x58060, 32> { };
		template <unsigned NR>
		struct Dc_rl_ch_5       : Register<0x58064+(NR*4), 32> { };
		struct Dc_gen           : Register<0x580d4, 32> { };
		struct Dc_disp_conf2_0  : Register<0x580e8, 32> { };
		struct Dc_disp_conf2_1  : Register<0x580ec, 32> { };
		template <unsigned NR>
		struct Dc_map_conf      : Register<0x58108+(NR*4), 32> { };
		template <unsigned NR>
		struct Dc_template      : Register<0x1080000+(NR*4), 32> { };


		/***********************************************
		 **  Display multi FIFO controller registers  **
		 ***********************************************/

		struct Dmfc_wr_chan     : Register<0x60004, 32> { };
		struct Dmfc_wr_chan_def : Register<0x60008, 32> { };
		struct Dmfc_dp_chan     : Register<0x6000c, 32> { };
		struct Dmfc_dp_chan_def : Register<0x60010, 32> { };
		struct Dmfc_general_1   : Register<0x60014, 32> { };
		struct Dmfc_ic_ctrl     : Register<0x6001c, 32> { };


		class Cp_mem
		{
			public:

				enum { OFFSET  = 0x1000000 };

				unsigned xv       : 10;  /* XV Virtual Coordinate */
				unsigned yv       : 9;   /* YV Virtual Coordinate */
				unsigned xb       : 13;  /* XB inner Block Coordinate */
				unsigned yb       : 12;  /* YB inner Block Coordinate */
				unsigned nsb_b    : 1;   /* New Sub Block */
				unsigned cf       : 1;   /* Current Field */
				unsigned sx       : 12;  /* Scroll X counter */
				unsigned sy       : 11;  /* Scroll Y counter */
				unsigned ns       : 10;  /* Number of Scroll */
				unsigned sdx      : 7;   /* Scroll Delta X */
				unsigned sm       : 10;  /* Scroll Max */
				unsigned scc      : 1;   /* Scrolling Configuration */
				unsigned sce      : 1;   /* Scrolling Enable */
				unsigned sdy      : 7;   /* Scroll Delta Y */
				unsigned sdrx     : 1;   /* Scroll Horizontal Direction */
				unsigned sdry     : 1;   /* Scroll Vertical Direction */
				unsigned bpp      : 3;   /* Bits per Pixel */
				unsigned dec_sel  : 2;   /* Decode Address Select */
				unsigned dim      : 1;   /* Access Dimension */
				unsigned so       : 1;   /* Scan Order */
				unsigned bndm     : 3;   /* Band Mode */
				unsigned bm       : 2;   /* Block Mode */
				unsigned rot      : 1;   /* Rotation */
				unsigned hf       : 1;   /* Horizontal Flip */
				unsigned vf       : 1;   /* Vertical Flip */
				unsigned the      : 1;   /* Threshold Enable */
				unsigned cap      : 1;   /* Conditional Access Polarity */
				unsigned cae      : 1;   /* Conditional Access Enable */
				unsigned fw       : 13;  /* Frame Width */
				unsigned fh       : 12;  /* Frame Height */
				unsigned res0     : 10;  /* reserved */
				Genode::uint32_t res1[3];

				unsigned eba0     : 29;  /* Ext Mem Buffer 0 Address */
				unsigned eba1     : 29;  /* Ext Mem Buffer 1 Address */
				unsigned ilo      : 20;  /* Interlace Offset */
				unsigned npb      : 7;   /* Number of Pixels in Whole Burst Access */
				unsigned pfs      : 4;   /* Pixel Format Select */
				unsigned alu      : 1;   /* Alpha Used */
				unsigned albm     : 3;   /* Alpha Channel Mapping */
				unsigned id       : 2;   /* AXI ID */
				unsigned th       : 7;   /* Threshold */
				unsigned sly      : 14;  /* Stride Line */
				unsigned wid0     : 3;   /* Width0 */
				unsigned wid1     : 3;   /* Width1 */
				unsigned wid2     : 3;   /* Width2 */
				unsigned wid3     : 3;   /* Width3 */
				unsigned off0     : 5;   /* Offset0 */
				unsigned off1     : 5;   /* Offset1 */
				unsigned off2     : 5;   /* Offset2 */
				unsigned off3     : 5;   /* Offset3 */
				unsigned sxys     : 1;   /* Select SX SY Set */
				unsigned cre      : 1;   /* Conditional Read Enable */
				unsigned dec_sel2 : 1;   /* Decode Address Select bit[2] */
				unsigned res2     : 9;   /* reserved */
				Genode::uint32_t res3[3];

				Cp_mem() { Genode::memset(this, 0, sizeof(Cp_mem)); }
		} __attribute__((packed));


		void _init_dma_channel(unsigned channel,
		                       Genode::uint16_t width, Genode::uint16_t height,
		                       Genode::uint32_t stride, Genode::addr_t phys_base)
		{
			void *dst =(void*)(base() + Cp_mem::OFFSET + channel*sizeof(Cp_mem));
			Cp_mem cpmem;

			cpmem.fw   = width  - 1;
			cpmem.fh   = height - 1;
			cpmem.sly  = stride - 1;
			cpmem.eba0 = phys_base >> 3;
			cpmem.eba1 = phys_base >> 3;
			cpmem.bpp  = 3;  /* corresponds to 16BPP      */
			cpmem.pfs  = 7;  /* corresponds to RGB        */
			cpmem.npb  = 31; /* 32 pixel per burst access */

			/* red */
			cpmem.wid0 = 4;
			cpmem.off0 = 0;

			/* green */
			cpmem.wid1 = 5;
			cpmem.off1 = 5;

			/* blue */
			cpmem.wid2 = 4;			cpmem.off2 = 11;

			/* alpha */
			cpmem.wid3 = 7;
			cpmem.off3 = 16;

			Genode::memcpy(dst, (void*)&cpmem, sizeof(Cp_mem));
		}


		void _init_di0(Genode::uint16_t width, Genode::uint16_t height,
		               Genode::uint32_t stride, Genode::addr_t phys_base)
		{
			/* set MCU_T to divide MCU access window into 2 */
			write<Disp_gen>(0x1600000); // ?= 0x600000

			/* link display controller events */
			write<Dc_rl_ch_5<0> >(0x5030000);
			write<Dc_rl_ch_5<2> >(0x602);
			write<Dc_rl_ch_5<4> >(0x701);
			write<Dc_rl_ch_5<0> >(0x5030000);
			write<Dc_rl_ch_5<1> >(0x0);
			write<Dc_rl_ch_5<1> >(0x0);
			write<Dc_rl_ch_5<2> >(0x602);
			write<Dc_rl_ch_5<3> >(0x0);
			write<Dc_rl_ch_5<3> >(0x0);

			write<Dc_wr_ch_conf_5>(0x2);
			write<Dc_wr_ch_addr_5>(0x0);

			write<Dc_gen>(0x84);


			/*************************
			 **  Display interface  **
			 *************************/

			/* clear DI */
			write<Di0::General>(0x200000);

			/* initialize display interface 0 */
			write<Di0::Bs_clkgen0>(0x38);
			write<Di0::Bs_clkgen1>(0x30000);
			write<Di0::Dw_gen<0> >(0x2020300);
			write<Di0::Dw_set3<0> >(0x60000);
			write<Di0::Sync_wave_gen0<0> >(0x21310000);
			write<Di0::Sync_wave_gen1<0> >(0x10000000);
			write<Di0::Sync_wave_gen0<1> >(0x21310001);
			write<Di0::Sync_wave_gen1<1> >(0x30141000);
			write<Di0::Step_repeat<0> >(0x0);
			write<Di0::Sync_wave_gen0<2> >(0x10520000);
			write<Di0::Sync_wave_gen1<2> >(0x30142000);
			write<Di0::Scr_conf>(0x20a);
			write<Di0::Sync_wave_gen0<3> >(0x3010b);
			write<Di0::Sync_wave_gen1<3> >(0x8000000);
			write<Di0::Step_repeat<1> >(0x1e00000);
			write<Di0::Sync_wave_gen0<4> >(0x10319);
			write<Di0::Sync_wave_gen1<4> >(0xa000000);
			write<Di0::Sync_wave_gen0<5> >(0x0);
			write<Di0::Sync_wave_gen1<5> >(0x0);
			write<Di0::Sync_wave_gen0<6> >(0x0);
			write<Di0::Sync_wave_gen1<6> >(0x0);
			write<Di0::Sync_wave_gen0<7> >(0x0);
			write<Di0::Sync_wave_gen1<7> >(0x0);
			write<Di0::Sync_wave_gen0<8> >(0x0);
			write<Di0::Sync_wave_gen1<8> >(0x0);
			write<Di0::Step_repeat<2> >(0x320);
			write<Di0::Step_repeat<3> >(0x0);
			write<Di0::Step_repeat<4> >(0x0);

			/* write display connection microcode */
			write<Dc_template<10> >(0x8885);
			write<Dc_template<11> >(0x380);
			write<Dc_template<12> >(0x8845);
			write<Dc_template<13> >(0x380);
			write<Dc_template<14> >(0x8805);
			write<Dc_template<15> >(0x380);

			write<Di0::General>(0x220000);
			write<Di0::Sync_as_gen>(0x2002);
			write<Di0::General>(0x200000);
			write<Di0::Sync_as_gen>(0x4002);

			write<Di0::Polarity>(0x10);
			write<Dc_disp_conf2_0>(0x320);

			/* init IDMAC channels */
			_init_dma_channel(CHAN_DP_PRIMARY_MAIN, width, height, stride, phys_base);
			_init_dma_channel(CHAN_DP_PRIMARY_AUXI, width, height, stride, phys_base);

			/* round robin for simultaneous synchronous flows from DC & DP */
			write<Dmfc_general_1>(0x3);

			/* enable DP, DI0, DC, DMFC */
			write<Conf>(0x660);

			/* use double buffer for main DMA channel */
			write<Ch_db_mode_sel0>(1 << CHAN_DP_PRIMARY_MAIN |
								   1 << CHAN_DP_PRIMARY_AUXI);

			/* buffer used by DMA channel is buffer 1 */
			write<Cur_buf_0>(1 << CHAN_DP_PRIMARY_MAIN);

			write<Dc_wr_ch_conf_5>(0x82);

			/* Enable IDMAC channels */
			write<Idmac_ch_en::Ch>(1, CHAN_DP_PRIMARY_MAIN);
			write<Idmac_ch_en::Ch>(1, CHAN_DP_PRIMARY_AUXI);
		}

		void _init_di1(Genode::uint16_t width, Genode::uint16_t height,
		               Genode::uint32_t stride, Genode::addr_t phys_base)
		{
			write<Disp_gen>(0x600000); //write<Disp_gen>(0x2400000);

			write<Dp_com_conf>(0);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);

			write<Dc_rl_ch_5<0> >(0x2030000);
			write<Dc_rl_ch_5<1> >(0);
			write<Dc_rl_ch_5<2> >(0x302);
			write<Dc_rl_ch_5<3> >(0);
			write<Dc_rl_ch_5<4> >(0x401);
			write<Dc_wr_ch_conf_5>(0xe);
			write<Dc_wr_ch_addr_5>(0x0);
			write<Dc_gen>(0x84);

			write<Conf>(0);

			write<Di1::General>(0x200000);
			write<Di1::General>(0x300000);

			write<Di1::Bs_clkgen0>(0x10);
			write<Di1::Bs_clkgen1>(0x10000);

			write<Pm>(0x10101010);

			write<Di1::Dw_gen<0> >(0x300);
			write<Di1::Dw_set3<0> >(0x20000);
			write<Di1::Sync_wave_gen0<0> >(0x29f90000);
			write<Di1::Sync_wave_gen1<0> >(0x10000000);
			write<Di1::Step_repeat<0> >(0x0);
			write<Di1::Sync_wave_gen0<1> >(0x29f90001);
			write<Di1::Sync_wave_gen1<1> >(0x30781000);
			write<Di1::Step_repeat<0> >(0x0);
			write<Di1::Sync_wave_gen0<2> >(0x192a0000);
			write<Di1::Sync_wave_gen1<2> >(0x30142000);
			write<Di1::Step_repeat<1> >(0x3000000);
			write<Di1::Scr_conf>(0x325);
			write<Di1::Sync_wave_gen0<3> >(0x300fb);
			write<Di1::Sync_wave_gen1<3> >(0x8000000);
			write<Di1::Step_repeat<1> >(0x3000000);
			write<Di1::Sync_wave_gen0<4> >(0x108c1);
			write<Di1::Sync_wave_gen1<4> >(0xa000000);
			write<Di1::Step_repeat<2> >(0x400);
			write<Di1::Sync_wave_gen0<6> >(0x29f90091);
			write<Di1::Sync_wave_gen1<6> >(0x30781000);
			write<Di1::Step_repeat<3> >(0x0);
			write<Di1::Sync_wave_gen0<7> >(0x192a000a);
			write<Di1::Sync_wave_gen1<7> >(0x30142000);
			write<Di1::Step_repeat<3> >(0x0);
			write<Di1::Sync_wave_gen0<5> >(0x0);
			write<Di1::Sync_wave_gen1<5> >(0x0);
			write<Di1::Sync_wave_gen0<8> >(0x0);
			write<Di1::Sync_wave_gen1<8> >(0x0);
			write<Di1::Step_repeat<4> >(0x0);
			write<Di1::Step_repeat<2> >(0x400);

			write<Di1::Sync_wave_gen0<5> >(0x90011);
			write<Di1::Sync_wave_gen1<5> >(0x4000000);
			write<Di1::Step_repeat<2> >(0x28a0400);

			write<Dc_template<4> >(0x10885);
			write<Dc_template<5> >(0x380);
			write<Dc_template<6> >(0x845);
			write<Dc_template<7> >(0x280);
			write<Dc_template<8> >(0x10805);
			write<Dc_template<9> >(0x380);

			write<Di1::General>(0x6300000);
			write<Di1::Sync_as_gen>(0x4000);

			write<Di1::Polarity>(0x10);
			write<Dc_disp_conf2_1>(0x400);

			_init_dma_channel(CHAN_DP_PRIMARY_MAIN, width, height, stride, phys_base);
			_init_dma_channel(CHAN_DP_PRIMARY_AUXI, width, height, stride, phys_base);

			/* use double buffer for main DMA channel */
			write<Ch_db_mode_sel0>(1 << CHAN_DP_PRIMARY_MAIN |
								   1 << CHAN_DP_PRIMARY_AUXI);

			/* buffer used by DMA channel is buffer 1 */
			write<Cur_buf_0>(1 << CHAN_DP_PRIMARY_MAIN);

			write<Conf>(0x6a0);

			/* Enable IDMAC channels */
			write<Idmac_ch_en::Ch>(1, CHAN_DP_PRIMARY_MAIN);
			write<Idmac_ch_en::Ch>(1, CHAN_DP_PRIMARY_AUXI);

			write<Idmac_wm_en>(1 << CHAN_DP_PRIMARY_MAIN |
							   1 << CHAN_DP_PRIMARY_AUXI);

			write<Dc_wr_ch_conf_5>(0x8e);

			write<Disp_gen>(0x2600000);
		}

	public:

		/**
		 * IPU initialization
		 */
		void init(Genode::uint16_t width, Genode::uint16_t height,
		          Genode::uint32_t stride, Genode::addr_t phys_base,
		          bool di0)
		{
			/* stop pixel clocks */
			write<Di0::General>(0);
			write<Di1::General>(0);

			/* reset IPU memory buffers */
			write<Mem_rst>(0x807fffff);
			while (read<Mem_rst>() & 0x80000000) ;

			/* initialize pixel format mappings for display controller */
			write<Dc_map_conf< 0> >(0x14830820);
			write<Dc_map_conf< 1> >(0x2d4920e6);
			write<Dc_map_conf< 2> >(0x39ac);
			write<Dc_map_conf<15> >(0xfff07ff);
			write<Dc_map_conf<16> >(0x5fc17ff);
			write<Dc_map_conf<17> >(0x11fc0bfc);
			write<Dc_map_conf<18> >(0x17ff0fff);
			write<Dc_map_conf<19> >(0x4f807ff);
			write<Dc_map_conf<20> >(0xff80afc);
			write<Dc_map_conf<21> >(0xdfc05fc);
			write<Dc_map_conf<22> >(0x15fc);

			/* clear interrupt control registers */
			write<Int_ctrl<4> >(0);
			write<Int_ctrl<5> >(0);
			write<Int_ctrl<8> >(0);
			write<Int_ctrl<9> >(0);

			/* disable DMFC channel from image converter */
			write<Dmfc_ic_ctrl>(0x2);

			/* set DMFC FIFO for idma channels */
			write<Dmfc_wr_chan>(0x90); /* channel CHAN_DC_SYNC_FLOW */
			write<Dmfc_wr_chan_def>(0x202020f6);
			write<Dmfc_dp_chan>(0x968a); /* channels CHAN_DP_PRIMARY_XXX */
			write<Dmfc_dp_chan_def>(0x2020f6f6);
			write<Dmfc_general_1>(0x3);

			/* set idma channels 23, 27, 28 as high priority */
			write<Idmac_ch_pri_1>(1 << CHAN_DP_PRIMARY_MAIN |
			                      1 << CHAN_DP_PRIMARY_AUXI |
			                      1 << CHAN_DC_SYNC_FLOW);

			/*
			 * generate 8 AXI bursts upon the assertion of DMA request
			 * for our channels
			 */
			write<Idmac_ch_lock_en_1>(0x3f0000);

			if (di0)
				_init_di0(width, height, stride, phys_base);
			else
				_init_di1(width, height, stride, phys_base);


			/************************
			 **  overlay settings  **
			 ************************/

			write<Dp_com_conf>(1 << 0);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);

			write<Dp_fg_pos_sync>(16);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);

			write<Dp_com_conf>(1 << 0 | 1 << 2);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);

			write<Gr_wnd_ctl_sync>(0xff000000);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);
		}


		void overlay(Genode::addr_t phys_base, int x, int y, int alpha)
		{
			volatile Genode::uint32_t *ptr = (volatile Genode::uint32_t*)
				(base() + Cp_mem::OFFSET + CHAN_DP_PRIMARY_AUXI*sizeof(Cp_mem));
			ptr[8] = (((phys_base >> 3) & 0b111) << 29) | (phys_base >> 3);
			ptr[9] = (phys_base >> 6);

			write<Dp_fg_pos_sync>(x << 16 | y);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);

			write<Gr_wnd_ctl_sync>(alpha << 24);
			write<Srm_pri2::Dp_m_srm>(Srm_pri2::Dp_m_srm::UPDATE_NOW);
		}


		/**
		 * Constructor
		 *
		 * \param mmio_base base address of IPU
		 */
		Ipu(Genode::addr_t mmio_base) : Genode::Mmio(mmio_base + REGS_OFF) { }
};

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__IMX53__IPU_H_ */
