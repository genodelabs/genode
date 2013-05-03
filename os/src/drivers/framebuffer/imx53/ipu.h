/*
 * \brief  Image Processing Unit registers
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-11-10
 */

#ifndef _IPU_H_
#define _IPU_H_

/* Genode includes */
#include <util/mmio.h>
#include <util/string.h>
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>


using namespace Genode;


struct Ipu : Genode::Mmio
{
	enum {
		REGS_OFF   = 0x06000000,
		CPMEM_OFF  = 0x01000000,
		IDMAC_CHAN = 23,
	};


	struct Conf            : Register<0x0,   32> { };
	struct Cur_buf_0       : Register<0x23c, 32> { };
	struct Int_ctrl_5      : Register<0x4c,  32> { };
	struct Int_ctrl_6      : Register<0x50,  32> { };
	struct Int_ctrl_9      : Register<0x5c,  32> { };
	struct Int_ctrl_10     : Register<0x60,  32> { };
	struct Srm_pri2        : Register<0xa4,  32> { };
	struct Disp_gen        : Register<0xc4,  32> { };
	struct Mem_rst         : Register<0xdc,  32> { };
	struct Ch_db_mode_sel0 : Register<0x150, 32> { };

	/**
	 * IDMAC cannel enable register
	 */
	struct Idmac_ch_en : Register_array<0x8004, 32, 32, 1>
	{
		struct Ch : Bitfield<0, 1> { };
	};

	struct Idmac_ch_pri_1   : Register<0x8014,    32> { };
	struct Dp_com_conf      : Register<0x18000,   32> { };
	struct Gr_wnd_ctl_sync  : Register<0x18004,   32> { };
	struct Di0_general      : Register<0x40000,   32> { };
	struct Di0_bs_clkgen0   : Register<0x40004,   32> { };
	struct Di0_bs_clkgen1   : Register<0x40008,   32> { };
	struct Di0_sw_gen0_1    : Register<0x4000c,   32> { };
	struct Di0_sw_gen0_2    : Register<0x40010,   32> { };
	struct Di0_sw_gen0_3    : Register<0x40014,   32> { };
	struct Di0_sw_gen0_4    : Register<0x40018,   32> { };
	struct Di0_sw_gen0_5    : Register<0x4001c,   32> { };
	struct Di0_sw_gen0_6    : Register<0x40020,   32> { };
	struct Di0_sw_gen0_7    : Register<0x40024,   32> { };
	struct Di0_sw_gen0_8    : Register<0x40028,   32> { };
	struct Di0_sw_gen0_9    : Register<0x4002c,   32> { };
	struct Di0_sw_gen1_1    : Register<0x40030,   32> { };
	struct Di0_sw_gen1_2    : Register<0x40034,   32> { };
	struct Di0_sw_gen1_3    : Register<0x40038,   32> { };
	struct Di0_sw_gen1_4    : Register<0x4003c,   32> { };
	struct Di0_sw_gen1_5    : Register<0x40040,   32> { };
	struct Di0_sw_gen1_6    : Register<0x40044,   32> { };
	struct Di0_sw_gen1_7    : Register<0x40048,   32> { };
	struct Di0_sw_gen1_8    : Register<0x4004c,   32> { };
	struct Di0_sw_gen1_9    : Register<0x40050,   32> { };
	struct Di0_sync_as_gen  : Register<0x40054,   32> { };
	struct Di0_dw_gen_1     : Register<0x40058,   32> { };
	struct Di0_dw_set3_1    : Register<0x40118,   32> { };
	struct Di0_stp_rep_1    : Register<0x40148,   32> { };
	struct Di0_stp_rep_3    : Register<0x4014c,   32> { };
	struct Di0_stp_rep_5    : Register<0x40150,   32> { };
	struct Di0_stp_rep_7    : Register<0x40154,   32> { };
	struct Di0_stp_rep_9    : Register<0x40158,   32> { };
	struct Di0_pol          : Register<0x40164,   32> { };
	struct Di0_scr_conf     : Register<0x40170,   32> { };
	struct Dc_wr_ch_conf_1  : Register<0x5801c,   32> { };
	struct Dc_wr_ch_conf_5  : Register<0x5805c,   32> { };
	struct Dc_wr_ch_addr_5  : Register<0x58060,   32> { };
	struct Dc_rl0_ch_5      : Register<0x58064,   32> { };
	struct Dc_rl1_ch_5      : Register<0x58068,   32> { };
	struct Dc_rl2_ch_5      : Register<0x5806c,   32> { };
	struct Dc_rl3_ch_5      : Register<0x58070,   32> { };
	struct Dc_rl4_ch_5      : Register<0x58074,   32> { };
	struct Dc_gen           : Register<0x580d4,   32> { };
	struct Dc_disp_conf2_0  : Register<0x580e8,   32> { };
	struct Dc_map_conf_0    : Register<0x58108,   32> { };
	struct Dc_map_conf_1    : Register<0x5810c,   32> { };
	struct Dc_map_conf_2    : Register<0x58110,   32> { };
	struct Dc_map_conf_15   : Register<0x58144,   32> { };
	struct Dc_map_conf_16   : Register<0x58148,   32> { };
	struct Dc_map_conf_17   : Register<0x5814c,   32> { };
	struct Dc_map_conf_18   : Register<0x58150,   32> { };
	struct Dc_map_conf_19   : Register<0x58154,   32> { };
	struct Dc_map_conf_20   : Register<0x58158,   32> { };
	struct Dc_map_conf_21   : Register<0x5815c,   32> { };
	struct Dc_map_conf_22   : Register<0x58160,   32> { };
	struct Dmfc_wr_chan     : Register<0x60004,   32> { };
	struct Dmfc_wr_chan_def : Register<0x60008,   32> { };
	struct Dmfc_dp_chan     : Register<0x6000c,   32> { };
	struct Dmfc_dp_chan_def : Register<0x60010,   32> { };
	struct Dmfc_general_1   : Register<0x60014,   32> { };
	struct Dmfc_ic_ctrl     : Register<0x6001c,   32> { };
	struct Dc_tmpl_low10    : Register<0x1080028, 32> { };
	struct Dc_tmpl_high10   : Register<0x108002c, 32> { };
	struct Dc_tmpl_low11    : Register<0x1080030, 32> { };
	struct Dc_tmpl_high11   : Register<0x1080034, 32> { };
	struct Dc_tmpl_low12    : Register<0x1080038, 32> { };
	struct Dc_tmpl_high12   : Register<0x108003c, 32> { };


	/**
	 * IDMAC channel parametrs memory structure
	 */
	struct Cp_mem
	{
		Genode::uint32_t Data[5];
		Genode::uint32_t Resetrved[3];
	} _ch_cpmem[2];

	void cpmem_set_field(Genode::uint8_t word, Genode::uint8_t bit,
	                     Genode::uint8_t size, Genode::uint32_t value)
	{
		int i = (bit) / 32;
		int off = (bit) % 32;
		_ch_cpmem[word].Data[i] |= (value) << off;
		if (((bit) + (size) - 1) / 32 > i) {
			_ch_cpmem[word].Data[i + 1] |= (value) >> (off ? (32 - off) : 0);
		}
	}

	/**
	 * IPU initialization
	 */
	void init(Genode::uint16_t width, Genode::uint16_t height,
	          Genode::uint32_t stride,
	          Genode::addr_t phys_base)
	{
		/* Reset ipu memory */
		write<Mem_rst>(0x807fffff);
		while (read<Mem_rst>() & 0x80000000)
			;

		/**
		 * Init display controller mappings
		 */
		write<Dc_map_conf_0>(0x14830820);
		write<Dc_map_conf_1>(0x2d4920e6);
		write<Dc_map_conf_2>(0x39ac);
		write<Dc_map_conf_15>(0xfff07ff);
		write<Dc_map_conf_16>(0x5fc17ff);
		write<Dc_map_conf_17>(0x11fc0bfc);
		write<Dc_map_conf_18>(0x17ff0fff);
		write<Dc_map_conf_19>(0x4f807ff);
		write<Dc_map_conf_20>(0xff80afc);
		write<Dc_map_conf_21>(0xdfc05fc);
		write<Dc_map_conf_22>(0x15fc);

		/**
		 * Clear interrupt control registers
		 */
		write<Int_ctrl_5>(0x0);
		write<Int_ctrl_6>(0x0);
		write<Int_ctrl_9>(0x0);
		write<Int_ctrl_10>(0x0);

		/**
		 * Init DMFC
		 */
		write<Dmfc_ic_ctrl>(0x2);
		write<Dmfc_wr_chan>(0x90);
		write<Dmfc_wr_chan_def>(0x202020f6);
		write<Dmfc_dp_chan>(0x9694);
		write<Dmfc_dp_chan_def>(0x2020f6f6);

		write<Idmac_ch_pri_1>(0x18800000);
		write<Gr_wnd_ctl_sync>(0x80000000);
		write<Srm_pri2>(0x605080b);
		write<Dp_com_conf>(0x4);
		write<Srm_pri2>(0x605080b);

		/**
		 * Link display controller events
		 */
		write<Dc_rl0_ch_5>(0x5030000);
		write<Dc_rl2_ch_5>(0x602);
		write<Dc_rl4_ch_5>(0x701);
		write<Dc_rl0_ch_5>(0x5030000);
		write<Dc_rl1_ch_5>(0x0);
		write<Dc_rl1_ch_5>(0x0);
		write<Dc_rl2_ch_5>(0x602);
		write<Dc_rl3_ch_5>(0x0);
		write<Dc_rl3_ch_5>(0x0);

		/**
		 * Init display controller
		 */
		write<Dc_wr_ch_conf_5>(0x2);
		write<Dc_wr_ch_addr_5>(0x0);
		write<Dc_gen>(0x84);

		write<Conf>(0x660);

		/**
		 * Init display interface
		 */
		write<Di0_bs_clkgen0>(0x38);
		write<Di0_bs_clkgen1>(0x30000);
		write<Di0_dw_gen_1>(0x2020300);
		write<Di0_dw_set3_1>(0x60000);
		write<Di0_sw_gen0_1>(0x21310000);
		write<Di0_sw_gen1_1>(0x10000000);
		write<Di0_sw_gen0_2>(0x21310001);
		write<Di0_sw_gen1_2>(0x30141000);
		write<Di0_stp_rep_1>(0x0);
		write<Di0_sw_gen0_3>(0x10520000);
		write<Di0_sw_gen1_3>(0x30142000);
		write<Di0_scr_conf>(0x20a);
		write<Di0_sw_gen0_4>(0x3010b);
		write<Di0_sw_gen1_4>(0x8000000);
		write<Di0_stp_rep_3>(0x1e00000);
		write<Di0_sw_gen0_5>(0x10319);
		write<Di0_sw_gen1_5>(0xa000000);
		write<Di0_sw_gen0_6>(0x0);
		write<Di0_sw_gen1_6>(0x0);
		write<Di0_sw_gen0_7>(0x0);
		write<Di0_sw_gen1_7>(0x0);
		write<Di0_sw_gen0_8>(0x0);
		write<Di0_sw_gen1_8>(0x0);
		write<Di0_sw_gen0_9>(0x0);
		write<Di0_sw_gen1_9>(0x0);
		write<Di0_stp_rep_5>(0x320);
		write<Di0_stp_rep_7>(0x0);
		write<Di0_stp_rep_9>(0x0);

		/**
		 * Write display connection templates
		 */
		write<Dc_tmpl_low10>(0x8885);
		write<Dc_tmpl_high10>(0x380);
		write<Dc_tmpl_low11>(0x8845);
		write<Dc_tmpl_high11>(0x380);
		write<Dc_tmpl_low12>(0x8805);
		write<Dc_tmpl_high12>(0x380);


		write<Di0_general>(0x220000);
		write<Di0_sync_as_gen>(0x2002);
		write<Di0_general>(0x200000);
		write<Di0_sync_as_gen>(0x4002);

		write<Di0_pol>(0x10);
		write<Dc_disp_conf2_0>(0x320);
		write<Dmfc_general_1>(0x3);
		write<Ch_db_mode_sel0>(0x800000);
		write<Cur_buf_0>(0x800000);

		write<Dc_wr_ch_conf_1>(0x4);
		write<Dc_wr_ch_conf_5>(0x82);
		write<Disp_gen>(0x1600000);

		/**
		 * Init IDMAC channel
		 */
		cpmem_set_field(0, 125, 13, width - 1);
		cpmem_set_field(0, 138, 12, height - 1);
		cpmem_set_field(1, 102, 14, stride - 1 );
		cpmem_set_field(1, 0, 29, 0);
		cpmem_set_field(1, 29, 29, phys_base >> 3);

		/* bits/pixel */
		cpmem_set_field(0, 107, 3, 3);

		/* pixel format RGB565 */
		cpmem_set_field(1, 85, 4, 7);

		/* burst size */
		cpmem_set_field(1, 78, 7, 15);


		/*******************
		 **  set packing  **
		 *******************/

		/* red */
		cpmem_set_field(1, 116, 3, 4);
		cpmem_set_field(1, 128, 5, 0);

		/* green */
		cpmem_set_field(1, 119, 3, 5);
		cpmem_set_field(1, 133, 5, 5);

		/* blue */
		cpmem_set_field(1, 122, 3, 4);
		cpmem_set_field(1, 138, 5, 11);

		/* alpha */
		cpmem_set_field(1, 125, 3, 7);
		cpmem_set_field(1, 143, 5, 16);

		cpmem_set_field(0, 46, 22, 0);
		cpmem_set_field(0, 68, 22, 0);

		Genode::memcpy((void *)(base + CPMEM_OFF + sizeof(_ch_cpmem) * IDMAC_CHAN),
		               (void *)&_ch_cpmem, sizeof(_ch_cpmem));

		write<Idmac_ch_en::Ch>(1, IDMAC_CHAN);
}

	/**
	 * Constructor
	 *
	 * \param mmio_base base address of IPU
	 */
	Ipu(Genode::addr_t mmio_base)
	: Genode::Mmio(mmio_base + REGS_OFF) { }
};

#endif /* _IPU_H_ */
