/*
 * \brief  Central clock module for i.MX8MQ
 * \author Stefan Kalkowski
 * \date   2020-06-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <os/attached_mmio.h>
#include <clock.h>

namespace Driver {
	using namespace Genode;

	struct Ccm;
};

struct Driver::Ccm
{
	class Frac_pll : public Driver::Clock, Mmio
	{
		struct Config_reg_0 : Register<0x0, 32>
		{
			struct Output_div_value : Bitfield<0, 5> {};
			struct Refclk_div_value : Bitfield<5, 6> {};
			struct Newdiv_ack       : Bitfield<11,1> {};
			struct Newdiv_val       : Bitfield<12,1> {};
			struct Bypass           : Bitfield<14,1> {};

			struct Ref_sel          : Bitfield<16,2>
			{
				enum Ref_clk {
					REF_CLK_25M, REF_CLK_27M, HDMI_PHY_27M, CLK_P_N };
			};

			struct Power_down       : Bitfield<19,1> {};
			struct Out_enable       : Bitfield<21,1> {};
			struct Lock_status      : Bitfield<31,1> {};
		};

		struct Config_reg_1 : Register<0x4, 32>
		{
			struct Int_div_ctl      : Bitfield<0,  7> {};
			struct Frac_div_ctl     : Bitfield<7, 24> {};
		};

		Clock_tree & _tree;

		Clock & _parent() const;

		public:

			Frac_pll(Name          name,
			         addr_t const  base,
			         Clock_tree  & tree);

			void          set_parent(Name name)   override;
			void          set_rate(unsigned long) override;
			unsigned long get_rate()        const override;
			void          enable()                override;
			void          disable()               override;
	};


	class Sccg_pll : public Driver::Clock, Mmio
	{
		struct Config_reg_0 : Register<0x0, 32>
		{
			struct Ref_sel : Bitfield<0,2> {
				enum Ref_clk {
					REF_CLK_25M, REF_CLK_27M, HDMI_PHY_27M, CLK_P_N };
			};
			struct Bypass2     : Bitfield<4, 1> {};
			struct Bypass1     : Bitfield<5, 1> {};
			struct Power_down  : Bitfield<7, 1> {};
			struct Out_enable  : Bitfield<25,1> {};
			struct Lock_status : Bitfield<31,1> {};
		};

		struct Config_reg_1 : Register<0x4, 32>
		{
			struct Sse : Bitfield<0,1> {};
		};

		struct Config_reg_2 : Register<0x8, 32>
		{
			struct Output_div_val : Bitfield<1, 6> {};
			struct Feedback_divf2 : Bitfield<7, 6> {};
			struct Feedback_divf1 : Bitfield<13,6> {};
			struct Ref_divr2      : Bitfield<19,6> {};
			struct Ref_divr1      : Bitfield<25,3> {};
		};

		Clock_tree & _tree;

		Clock & _parent() const;

		public:

			Sccg_pll(Name          name,
			         addr_t const  base,
			         Clock_tree  & tree)
			: Clock(name, tree), Mmio(base), _tree(tree) {}

			void          set_parent(Name name)   override;
			void          set_rate(unsigned long) override;
			unsigned long get_rate()        const override;
			void          enable()                override;
			void          disable()               override;
	};


	class Root_clock : public Clock, Mmio
	{
		struct Target_reg : Register<0x0, 32>
		{
			struct Post_div : Bitfield<0,6>  {};
			struct Pre_div  : Bitfield<16,3> {};
			struct Ref_sel  : Bitfield<24,3> {};
			struct Enable   : Bitfield<28,1> {};
		};

		struct Clock_ref {
			Clock & ref;
			Clock_ref(Clock & c) : ref(c) {}
		};

		enum { REF_CLK_MAX = 8 };

		Clock_tree & _tree;
		Clock_ref    _ref_clks[REF_CLK_MAX];

		Clock & _parent() const;

		public:

			Root_clock(Name          name,
			           addr_t const  base,
			           Clock       & ref_clk0,
			           Clock       & ref_clk1,
			           Clock       & ref_clk2,
			           Clock       & ref_clk3,
			           Clock       & ref_clk4,
			           Clock       & ref_clk5,
			           Clock       & ref_clk6,
			           Clock       & ref_clk7,
			           Clock_tree  & tree)
			: Clock(name, tree), Mmio(base), _tree(tree),
			  _ref_clks { ref_clk0, ref_clk1, ref_clk2, ref_clk3,
			              ref_clk4, ref_clk5, ref_clk6, ref_clk7 }{}

			void          set_parent(Name name)   override;
			void          set_rate(unsigned long) override;
			unsigned long get_rate()        const override;
			void          enable()                override;
			void          disable()               override;
	};


	class Root_clock_divider : public Clock, Mmio
	{
		struct Target_reg : Register<0x0, 32>
		{
			struct Post_div : Bitfield<0,6>  {};
		};


		Clock      & _parent;

		public:

			Root_clock_divider(Name          name,
			                   addr_t const  base,
			                   Clock       & parent,
			                   Clock_tree  & tree)
			: Clock(name, tree), Mmio(base),
			  _parent(parent) {}

			void          set_rate(unsigned long) override;
			unsigned long get_rate()        const override;
	};


	class Gate : public Clock, Mmio
	{
		struct Ccgr : Register<0x0, 32> { };

		Clock & _parent;

		public:

			Gate(Name          name,
			     addr_t const  base,
			     Clock       & parent,
			     Clock_tree  & tree)
			: Clock(name, tree), Mmio(base), _parent(parent) {}

			void          set_rate(unsigned long) override {}
			unsigned long get_rate()        const override {
				return _parent.get_rate(); }

			void enable()  override;
			void disable() override;
	};


	enum {
		CCM_MMIO_BASE        = 0x30380000,
		CCM_MMIO_SIZE        = 0x10000,
		CCM_ANALOG_MMIO_BASE = 0x30360000,
		CCM_ANALOG_MMIO_SIZE = 0x10000,
	};

	Ccm(Genode::Env & env);

	Genode::Env     & env;
	Attached_mmio     ccm_regs        { env, CCM_MMIO_BASE, CCM_MMIO_SIZE };
	Attached_mmio     ccm_analog_regs { env, CCM_ANALOG_MMIO_BASE, CCM_ANALOG_MMIO_SIZE };
	Clock::Clock_tree tree            { };

	addr_t frac_pll_base(unsigned pll) {
		return (addr_t)ccm_analog_regs.local_addr<const void>() + pll*0x8; }

	addr_t sccg_pll_base(unsigned pll) {
		return (addr_t)ccm_analog_regs.local_addr<const void>() + 0x30 + pll*0xc; }

	addr_t gate_base(unsigned nr) {
		return (addr_t)ccm_regs.local_addr<const void>() + 0x4000 + nr*0x10; }

	addr_t root_base(unsigned nr) {
		return (addr_t)ccm_regs.local_addr<const void>() + 0x8000 + nr*0x80; }

	Fixed_clock   no_clk            { "no_clk",                            0,   tree };
	Fixed_clock   k32_ref_clk       { "32k_ref_clk",               32 * 1000,   tree };
	Fixed_clock   m25_ref_clk       { "25m_ref_clk",        25 * 1000 * 1000,   tree };
	Fixed_clock   m27_ref_clk       { "27m_ref_clk",        27 * 1000 * 1000,   tree };
	Fixed_clock   hdmi_phy_m27_clk  { "hdmi_phy_27m_clk",   27 * 1000 * 1000,   tree };
	Fixed_clock   ext_clk_1         { "ext_clk_1",         133 * 1000 * 1000,   tree };
	Fixed_clock   ext_clk_2         { "ext_clk_2",         133 * 1000 * 1000,   tree };
	Fixed_clock   ext_clk_3         { "ext_clk_3",         133 * 1000 * 1000,   tree };
	Fixed_clock   ext_clk_4         { "ext_clk_4",         133 * 1000 * 1000,   tree };

	Frac_pll      audio_pll1_clk    { "audio_pll1_clk",    frac_pll_base(0),    tree };
	Frac_pll      audio_pll2_clk    { "audio_pll2_clk",    frac_pll_base(1),    tree };
	Frac_pll      video_pll1_clk    { "video_pll1_clk",    frac_pll_base(2),    tree };
	Frac_pll      gpu_pll_clk       { "gpu_pll_clk",       frac_pll_base(3),    tree };
	Frac_pll      vpu_pll_clk       { "vpu_pll_clk",       frac_pll_base(4),    tree };
	Frac_pll      arm_pll_clk       { "arm_pll_clk",       frac_pll_base(5),    tree };

	Sccg_pll      system_pll1_clk   { "system_pll1_clk",   sccg_pll_base(0),    tree };
	Sccg_pll      system_pll2_clk   { "system_pll2_clk",   sccg_pll_base(1),    tree };
	Sccg_pll      system_pll3_clk   { "system_pll3_clk",   sccg_pll_base(2),    tree };
	Sccg_pll      video_pll2_clk    { "video2_pll2_clk",   sccg_pll_base(3),    tree };
	Sccg_pll      dram_pll_clk      { "dram_pll_clk",      sccg_pll_base(4),    tree };

	Fixed_divider system_pll1_div20 { "system_pll1_div20", system_pll1_clk, 20, tree };
	Fixed_divider system_pll1_div10 { "system_pll1_div10", system_pll1_clk, 10, tree };
	Fixed_divider system_pll1_div8  { "system_pll1_div8",  system_pll1_clk,  8, tree };
	Fixed_divider system_pll1_div6  { "system_pll1_div6",  system_pll1_clk,  6, tree };
	Fixed_divider system_pll1_div5  { "system_pll1_div5",  system_pll1_clk,  5, tree };
	Fixed_divider system_pll1_div4  { "system_pll1_div4",  system_pll1_clk,  4, tree };
	Fixed_divider system_pll1_div3  { "system_pll1_div3",  system_pll1_clk,  3, tree };
	Fixed_divider system_pll1_div2  { "system_pll1_div2",  system_pll1_clk,  2, tree };
	Fixed_divider system_pll2_div20 { "system_pll2_div20", system_pll2_clk, 20, tree };
	Fixed_divider system_pll2_div10 { "system_pll2_div10", system_pll2_clk, 10, tree };
	Fixed_divider system_pll2_div8  { "system_pll2_div8",  system_pll2_clk,  8, tree };
	Fixed_divider system_pll2_div6  { "system_pll2_div6",  system_pll2_clk,  6, tree };
	Fixed_divider system_pll2_div5  { "system_pll2_div5",  system_pll2_clk,  5, tree };
	Fixed_divider system_pll2_div4  { "system_pll2_div4",  system_pll2_clk,  4, tree };
	Fixed_divider system_pll2_div3  { "system_pll2_div3",  system_pll2_clk,  3, tree };
	Fixed_divider system_pll2_div2  { "system_pll2_div2",  system_pll2_clk,  2, tree };

	Root_clock arm_a53_clk_root { "arm_a53_clk_root", root_base(0), m25_ref_clk, arm_pll_clk, system_pll2_div2, system_pll2_clk, system_pll1_clk, system_pll1_div2, audio_pll1_clk, system_pll3_clk, tree };
	Root_clock arm_m4_clk_root { "arm_m4_clk_root", root_base(1), m25_ref_clk, system_pll2_div5, system_pll2_div4, system_pll1_div3, system_pll1_clk, audio_pll1_clk, video_pll1_clk, system_pll3_clk, tree };
	Root_clock vpu_a53_clk_root { "vpu_a53_clk_root", root_base(2), m25_ref_clk, arm_pll_clk, system_pll2_div2, system_pll2_clk, system_pll1_clk, system_pll1_div2, audio_pll1_clk, vpu_pll_clk, tree };
	Root_clock gpu_core_clk_root { "gpu_core_clk_root", root_base(3), m25_ref_clk, gpu_pll_clk, system_pll1_clk, system_pll3_clk, system_pll2_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock gpu_shader_clk_root { "gpu_shader_clk", root_base(4), m25_ref_clk, gpu_pll_clk, system_pll1_clk, system_pll3_clk, system_pll2_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock main_axi_clk_root { "main_axi_clk_root", root_base(16), m25_ref_clk, system_pll2_div3, system_pll1_clk, system_pll2_div4, system_pll2_clk, audio_pll1_clk, video_pll1_clk, system_pll1_div8, tree };
	Root_clock enet_axi_clk_root { "enet_axi_clk_root", root_base(17), m25_ref_clk, system_pll1_div3, system_pll1_clk, system_pll2_div4, system_pll2_div5, audio_pll1_clk, video_pll1_clk, system_pll3_clk, tree };
	Root_clock nand_usdhc_bus_clk_root { "nand_usdhc_bus_clk_root", root_base(18), m25_ref_clk, system_pll1_div3, system_pll1_clk, system_pll2_div5, system_pll1_div6, system_pll3_clk, system_pll2_div4, audio_pll1_clk, tree };
	Root_clock vpu_bus_clk_root { "vpu_bus_clk_root", root_base(19), m25_ref_clk, system_pll1_clk, vpu_pll_clk, audio_pll2_clk, system_pll3_clk, system_pll2_clk, system_pll2_div5, system_pll1_div8, tree };
	Root_clock display_axi_clk_root { "display_axi_clk_root", root_base(20), m25_ref_clk, system_pll2_div8, system_pll1_clk, system_pll3_clk, system_pll1_div20, audio_pll2_clk, ext_clk_1, ext_clk_4, tree };
	Root_clock display_apb_clk_root { "display_apb_clk_root", root_base(21), m25_ref_clk, system_pll2_div8, system_pll1_clk, system_pll3_clk, system_pll1_div20, audio_pll2_clk, ext_clk_1, ext_clk_3, tree };
	Root_clock display_rtrm_clk_root { "display_rtrm_clk_root", root_base(22), m25_ref_clk, system_pll1_clk, system_pll2_div5, system_pll1_div2, audio_pll1_clk, video_pll1_clk, ext_clk_2, ext_clk_3, tree };
	Root_clock usb_bus_clk_root { "usb_bus_clk_root", root_base(23), m25_ref_clk, system_pll2_div2, system_pll1_clk, system_pll2_div10, system_pll2_div5, ext_clk_2, ext_clk_4, audio_pll2_clk, tree };
	Root_clock gpu_axi_clk_root { "gpu_axi_clk_root", root_base(24), m25_ref_clk, system_pll1_clk, gpu_pll_clk, system_pll3_clk, system_pll2_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock gpu_ahb_clk_root { "gpu_ahb_clk_root", root_base(25), m25_ref_clk, system_pll1_clk, gpu_pll_clk, system_pll3_clk, system_pll2_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock noc_clk_root { "noc_clk_root", root_base(26), m25_ref_clk, system_pll1_clk, system_pll3_clk, system_pll2_clk, system_pll2_div2, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock noc_apb_clk_root { "noc_apb_clk_root", root_base(27), m25_ref_clk, system_pll1_div2, system_pll3_clk, system_pll2_div3, system_pll2_div5, system_pll1_clk, audio_pll1_clk, video_pll1_clk, tree };
	Root_clock ahb_clk_root { "ahb_clk_root", root_base(32), m25_ref_clk, system_pll1_div6, system_pll1_clk, system_pll1_div2, system_pll2_div8, system_pll3_clk, audio_pll1_clk, video_pll1_clk, tree };
	Root_clock audio_ahb_clk_root { "audio_ahb_clk_root", root_base(34), m25_ref_clk, system_pll2_div2, system_pll1_clk, system_pll2_clk, system_pll2_div6, system_pll3_clk, audio_pll1_clk, video_pll1_clk, tree };
	Root_clock mipi_dsi_esc_rx_clk_root { "mipi_dsi_esc_rx_clk_root", root_base(36), m25_ref_clk, system_pll2_div10, system_pll1_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_3, audio_pll2_clk, tree };
	Root_clock dram_alt_clk_root { "dram_alt_clk_root", root_base(64), m25_ref_clk, system_pll1_clk, system_pll1_div8, system_pll2_div2, system_pll2_div4, system_pll1_div2, audio_pll1_clk, system_pll1_div3, tree };
	Root_clock dram_apb_clk_root { "dram_apb_clk_root", root_base(65), m25_ref_clk, system_pll2_div5, system_pll1_div20, system_pll1_div5, system_pll1_clk, system_pll3_clk, system_pll2_div4, audio_pll2_clk, tree };
	Root_clock vpu_g1_clk_root { "vpu_g1_clk_root", root_base(66), m25_ref_clk, vpu_pll_clk, system_pll1_clk, system_pll2_clk, system_pll1_div8, system_pll2_div8, system_pll3_clk, audio_pll1_clk, tree };
	Root_clock vpu_g2_clk_root { "vpu_g2_clk_root", root_base(67), m25_ref_clk, vpu_pll_clk, system_pll1_clk, system_pll2_clk, system_pll1_div8, system_pll2_div8, system_pll3_clk, audio_pll1_clk, tree };
	Root_clock display_dtrc_clk_root { "display_dtrc_clk_root", root_base(68), m25_ref_clk, video_pll2_clk, system_pll1_clk, system_pll2_clk, system_pll1_div5, video_pll1_clk, system_pll3_clk, audio_pll2_clk, tree };
	Root_clock display_dc8000_clk_root { "display_dc8000_clk_root", root_base(69), m25_ref_clk, video_pll2_clk, system_pll1_clk, system_pll2_clk, system_pll1_div5, video_pll1_clk, system_pll3_clk, audio_pll2_clk, tree };
	Root_clock pcie1_ctrl_clk_root { "pcie1_ctrl_clk_root", root_base(70), m25_ref_clk, system_pll2_div4, system_pll2_div5, system_pll1_div3, system_pll1_clk, system_pll2_div2, system_pll2_div3, system_pll3_clk, tree };
	Root_clock pcie1_phy_clk_root { "pcie1_phy_clk_root", root_base(71), m25_ref_clk, system_pll2_div10, system_pll2_div2, ext_clk_1, ext_clk_2, ext_clk_3, ext_clk_4, system_pll1_div2, tree };
	Root_clock pcie1_aux_clk_root { "pcie1_aux_clk_root", root_base(72), m25_ref_clk, system_pll2_div5, system_pll2_div20, system_pll3_clk, system_pll2_div10, system_pll1_div10, system_pll1_div5, system_pll1_div4, tree };
	Root_clock dc_pixel_clk_root { "dc_pixel_clk_root", root_base(73), m25_ref_clk, video_pll1_clk, audio_pll2_clk, audio_pll1_clk, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_4, tree };
	Root_clock lcdif_pixel_clk_root { "lcdif_pixel_clk_root", root_base(74), m25_ref_clk, video_pll1_clk, audio_pll2_clk, audio_pll1_clk, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_4, tree };
	Root_clock sai1_clk_root { "sai1_clk_root", root_base(75), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_1, ext_clk_2, tree };
	Root_clock sai2_clk_root { "sai2_clk_root", root_base(76), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_2, ext_clk_3, tree };
	Root_clock sai3_clk_root { "sai3_clk_root", root_base(77), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_3, ext_clk_4, tree };
	Root_clock sai4_clk_root { "sai4_clk_root", root_base(78), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_1, ext_clk_2, tree };
	Root_clock sai5_clk_root { "sai5_clk_root", root_base(79), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_2, ext_clk_3, tree };
	Root_clock sai6_clk_root { "sai6_clk_root", root_base(80), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_3, ext_clk_4, tree };
	Root_clock spdif1_clk_root { "spdif1_clk_root", root_base(81), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_2, ext_clk_3, tree };
	Root_clock spdif2_clk_root { "spdif2_clk_root", root_base(82), m25_ref_clk, audio_pll1_clk, audio_pll2_clk, video_pll1_clk, system_pll1_div6, m27_ref_clk, ext_clk_3, ext_clk_4, tree };
	Root_clock enet_ref_clk_root { "enet_ref_clk_root", root_base(83), m25_ref_clk, system_pll2_div8, system_pll2_div20, system_pll2_div10, system_pll1_div5, audio_pll1_clk, video_pll1_clk, ext_clk_4, tree };
	Root_clock enet_timer_clk_root { "enet_timer_clk_root", root_base(84), m25_ref_clk, system_pll2_div10, audio_pll1_clk, ext_clk_1, ext_clk_2, ext_clk_3, ext_clk_4, video_pll1_clk, tree };
	Root_clock enet_phy_ref_clk_root { "enet_phy_ref_clk_root", root_base(85), m25_ref_clk, system_pll2_div20, system_pll2_div8, system_pll2_div5, system_pll2_div2, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, tree };
	Root_clock nand_clk_root { "nand_clk_root", root_base(86), m25_ref_clk, system_pll2_div2, audio_pll1_clk, system_pll1_div2, audio_pll2_clk, system_pll3_clk, system_pll2_div4, video_pll1_clk, tree };
	Root_clock qspi_clk_root { "qspi_clk_root", root_base(87), m25_ref_clk, system_pll1_div2, system_pll1_clk, system_pll2_div2, audio_pll2_clk, system_pll1_div3, system_pll3_clk, system_pll1_div8, tree };
	Root_clock usdhc1_clk_root { "usdhc1_clk_root", root_base(88), m25_ref_clk, system_pll1_div2, system_pll1_clk, system_pll2_div2, system_pll3_clk, system_pll1_div3, audio_pll2_clk, system_pll1_div8, tree };
	Root_clock usdhc2_clk_root { "usdhc2_clk_root", root_base(89), m25_ref_clk, system_pll1_div2, system_pll1_clk, system_pll2_div2, system_pll3_clk, system_pll1_div3, audio_pll2_clk, system_pll1_div8, tree };
	Root_clock i2c1_clk_root { "i2c1_clk_root", root_base(90), m25_ref_clk, system_pll1_div5, system_pll2_div20, system_pll3_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, system_pll1_div6, tree };
	Root_clock i2c2_clk_root { "i2c2_clk_root", root_base(91), m25_ref_clk, system_pll1_div5, system_pll2_div20, system_pll3_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, system_pll1_div6, tree };
	Root_clock i2c3_clk_root { "i2c3_clk_root", root_base(92), m25_ref_clk, system_pll1_div5, system_pll2_div20, system_pll3_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, system_pll1_div6, tree };
	Root_clock i2c4_clk_root { "i2c4_clk_root", root_base(93), m25_ref_clk, system_pll1_div5, system_pll2_div20, system_pll3_clk, audio_pll1_clk, video_pll1_clk, audio_pll2_clk, system_pll1_div6, tree };
	Root_clock uart1_clk_root { "uart1_clk_root", root_base(94), m25_ref_clk, system_pll1_div10, system_pll2_div5, system_pll2_div10, system_pll3_clk, ext_clk_2, ext_clk_4, audio_pll2_clk, tree };
	Root_clock uart2_clk_root { "uart2_clk_root", root_base(95), m25_ref_clk, system_pll1_div10, system_pll2_div5, system_pll2_div10, system_pll3_clk, ext_clk_2, ext_clk_3, audio_pll2_clk, tree };
	Root_clock uart3_clk_root { "uart3_clk_root", root_base(96), m25_ref_clk, system_pll1_div10, system_pll2_div5, system_pll2_div10, system_pll3_clk, ext_clk_2, ext_clk_4, audio_pll2_clk, tree };
	Root_clock uart4_clk_root { "uart4_clk_root", root_base(97), m25_ref_clk, system_pll1_div10, system_pll2_div5, system_pll2_div10, system_pll3_clk, ext_clk_2, ext_clk_3, audio_pll2_clk, tree };
	Root_clock usb_core_ref_clk_root { "usb_core_ref_clk_root", root_base(98), m25_ref_clk, system_pll1_div8, system_pll1_div20, system_pll2_div10, system_pll2_div5, ext_clk_2, ext_clk_3, audio_pll2_clk, tree };
	Root_clock usb_phy_ref_clk_root { "usb_phy_ref_clk_root", root_base(99), m25_ref_clk, system_pll1_div8, system_pll1_div20, system_pll2_div10, system_pll2_div5, ext_clk_2, ext_clk_3, audio_pll2_clk, tree };
	Root_clock gic_clk_root { "gic_clk_root", root_base(100), m25_ref_clk, system_pll2_div5, system_pll1_div20, system_pll2_div10, system_pll1_clk, ext_clk_2, ext_clk_4, audio_pll2_clk, tree };
	Root_clock ecspi1_clk_root { "ecspi1_clk_root", root_base(101), m25_ref_clk, system_pll2_div5, system_pll1_div20, system_pll1_div5, system_pll1_clk, system_pll3_clk, system_pll2_div4, audio_pll2_clk, tree };
	Root_clock ecspi2_clk_root { "ecspi2_clk_root", root_base(102), m25_ref_clk, system_pll2_div5, system_pll1_div20, system_pll1_div5, system_pll1_clk, system_pll3_clk, system_pll2_div4, audio_pll2_clk, tree };
	Root_clock pwm1_clk_root { "pwm1_clk_root", root_base(103), m25_ref_clk, system_pll2_div10, system_pll1_div5, system_pll1_div20, system_pll3_clk, ext_clk_1, system_pll1_div10, video_pll1_clk, tree };
	Root_clock pwm2_clk_root { "pwm2_clk_root", root_base(104), m25_ref_clk, system_pll2_div10, system_pll1_div5, system_pll1_div20, system_pll3_clk, ext_clk_1, system_pll1_div10, video_pll1_clk, tree };
	Root_clock pwm3_clk_root { "pwm3_clk_root", root_base(105), m25_ref_clk, system_pll2_div10, system_pll1_div5, system_pll1_div20, system_pll3_clk, ext_clk_2, system_pll1_div10, video_pll1_clk, tree };
	Root_clock pwm4_clk_root { "pwm4_clk_root", root_base(106), m25_ref_clk, system_pll2_div10, system_pll1_div5, system_pll1_div20, system_pll3_clk, ext_clk_2, system_pll1_div10, video_pll1_clk, tree };
	Root_clock gpt1_clk_root { "gpt1_clk_root", root_base(107), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_1, tree };
	Root_clock gpt2_clk_root { "gpt2_clk_root", root_base(108), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_2, tree };
	Root_clock gpt3_clk_root { "gpt3_clk_root", root_base(109), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_3, tree };
	Root_clock gpt4_clk_root { "gpt4_clk_root", root_base(110), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_1, tree };
	Root_clock gpt5_clk_root { "gpt5_clk_root", root_base(111), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_2, tree };
	Root_clock gpt6_clk_root { "gpt6_clk_root", root_base(112), m25_ref_clk, system_pll2_div10, system_pll1_div2, system_pll1_div20, video_pll1_clk, system_pll1_div10, audio_pll1_clk, ext_clk_3, tree };
	Root_clock trace_clk_root { "trace_clk_root", root_base(113), m25_ref_clk, system_pll1_div6, system_pll1_div5, vpu_pll_clk, system_pll2_div8, system_pll3_clk, ext_clk_1, ext_clk_3, tree };
	Root_clock wdog_clk_root { "wdog_clk_root", root_base(114), m25_ref_clk, system_pll1_div6, system_pll1_div5, vpu_pll_clk, system_pll2_div8, system_pll3_clk, system_pll1_div10, system_pll2_div6, tree };
	Root_clock wrclk_clk_root { "wrclk_clk_root", root_base(115), m25_ref_clk, system_pll1_div20, vpu_pll_clk, system_pll3_clk, system_pll2_div5, system_pll1_div3, system_pll2_div2, system_pll1_div8, tree };
	Root_clock ipp_do_clko1clk_root { "ipp_do_clko1_clk_root", root_base(116), m25_ref_clk, system_pll1_clk, m27_ref_clk, system_pll1_div4, audio_pll2_clk, system_pll2_div2, vpu_pll_clk, system_pll1_div10, tree };
	Root_clock ipp_do_clko2_clk_root { "ipp_do_clko2_clk_root", root_base(117), m25_ref_clk, system_pll2_div5, system_pll1_div2, system_pll2_div6, system_pll3_clk, audio_pll1_clk, video_pll1_clk, k32_ref_clk, tree };
	Root_clock mipi_dsi_core_clk_root { "mipi_dsi_core_clk_root", root_base(118), m25_ref_clk, system_pll1_div3, system_pll2_div4, system_pll1_clk, system_pll2_clk, system_pll3_clk, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_dsi_phy_ref_clk_root { "mipi_dsi_phy_ref_clk_root", root_base(119), m25_ref_clk, system_pll2_div8, system_pll2_div10, system_pll1_clk, system_pll2_clk, ext_clk_2, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_dsi_dbi_clk_root { "mipi_dsi_dbi_clk_root", root_base(120), m25_ref_clk, system_pll1_div3, system_pll2_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock old_mipi_dsi_esc_clk_root { "old_mipi_dsi_esc_clk_root", root_base(121), m25_ref_clk, system_pll2_div10, system_pll1_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_3, audio_pll2_clk, tree };
	Root_clock mipi_csi1_core_clk_root { "mipi_csi1_core_clk_root", root_base(122), m25_ref_clk, system_pll1_div3, system_pll2_div4, system_pll1_clk, system_pll2_clk, system_pll3_clk, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_csi1_phy_ref_clk_root { "mipi_csi1_phy_ref_clk_root", root_base(123), m25_ref_clk, system_pll2_div3, system_pll2_div10, system_pll1_clk, system_pll2_clk, ext_clk_2, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_csi1_esc_clk_root { "mipi_csi1_esc_clk_root", root_base(124), m25_ref_clk, system_pll2_div10, system_pll1_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_3, audio_pll2_clk, tree };
	Root_clock mipi_csi2_core_clk_root { "mipi_csi2_core_clk_root", root_base(125), m25_ref_clk, system_pll1_div3, system_pll2_div4, system_pll1_clk, system_pll2_clk, system_pll3_clk, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_csi2_phy_ref_clk_root { "mipi_csi2_phy_ref_clk_root", root_base(126), m25_ref_clk, system_pll2_div3, system_pll2_div10, system_pll1_clk, system_pll2_clk, ext_clk_2, audio_pll2_clk, video_pll1_clk, tree };
	Root_clock mipi_csi2_esc_clk_root { "mipi_csi2_esc_clk_root", root_base(127), m25_ref_clk, system_pll2_div10, system_pll1_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_3, audio_pll2_clk, tree };
	Root_clock pcie2_ctrl_clk_root { "pcie2_ctrl_clk_root", root_base(128), m25_ref_clk, system_pll2_div4, system_pll2_div5, system_pll1_div3, system_pll1_clk, system_pll2_div2, system_pll2_div3, system_pll3_clk, tree };
	Root_clock pcie2_phy_clk_root { "pcie2_phy_clk_root", root_base(129), m25_ref_clk, system_pll2_div10, system_pll2_div2, ext_clk_1, ext_clk_2, ext_clk_3, ext_clk_4, system_pll1_div2, tree };
	Root_clock pcie2_aux_clk_root { "pcie2_aux_clk_root", root_base(130), m25_ref_clk, system_pll2_div5, system_pll2_div20, system_pll3_clk, system_pll2_div10, system_pll1_div10, system_pll1_div5, system_pll1_div4, tree };
	Root_clock ecspi3_clk_root { "ecspi3_clk_root", root_base(131), m25_ref_clk, system_pll2_div5, system_pll1_div20, system_pll1_div5, system_pll1_clk, system_pll3_clk, system_pll2_div4, audio_pll2_clk, tree };
	Root_clock old_mipi_dsi_esc_rx_clk_root { "old_mipi_dsi_esc_rx_clk_root", root_base(132), m25_ref_clk, system_pll2_div10, system_pll1_div10, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_3, audio_pll2_clk, tree };
	Root_clock display_hdmi_clk_root { "display_hdmi_clk_root", root_base(133), m25_ref_clk, system_pll1_div4, system_pll2_div5, vpu_pll_clk, system_pll1_clk, system_pll2_clk, system_pll3_clk, ext_clk_4, tree };

	Root_clock_divider ipg_clk_root          { "ipg_clk_root",          root_base(33), ahb_clk_root,             tree };
	Root_clock_divider ipg_audio_clk_root    { "ipg_audio_clk_root",    root_base(35), audio_ahb_clk_root,       tree };
	Root_clock_divider mipi_dsi_esc_clk_root { "mipi_dsi_esc_clk_root", root_base(37), mipi_dsi_esc_rx_clk_root, tree };

	Gate ecspi1_gate     { "ecspi1_gate",     gate_base(7),   ecspi1_clk_root,         tree };
	Gate ecspi2_gate     { "ecspi2_gate",     gate_base(8),   ecspi2_clk_root,         tree };
	Gate ecspi3_gate     { "ecspi3_gate",     gate_base(9),   ecspi3_clk_root,         tree };
	Gate enet1_gate      { "enet1_gate",      gate_base(10),  enet_axi_clk_root,       tree };
	Gate gpt1_gate       { "gpt1_gate",       gate_base(16),  gpt1_clk_root,           tree };
	Gate i2c1_gate       { "i2c1_gate",       gate_base(23),  i2c1_clk_root,           tree };
	Gate i2c2_gate       { "i2c2_gate",       gate_base(24),  i2c2_clk_root,           tree };
	Gate i2c3_gate       { "i2c3_gate",       gate_base(25),  i2c3_clk_root,           tree };
	Gate i2c4_gate       { "i2c4_gate",       gate_base(26),  i2c4_clk_root,           tree };
	Gate mu_gate         { "mu_gate",         gate_base(33),  ipg_clk_root,            tree };
	Gate ocotp_gate      { "ocotp_gate",      gate_base(34),  ipg_clk_root,            tree };
	Gate pcie_gate       { "pcie_gate",       gate_base(37),  pcie1_ctrl_clk_root,     tree };
	Gate pwm1_gate       { "pwm1_gate",       gate_base(40),  pwm1_clk_root,           tree };
	Gate pwm2_gate       { "pwm2_gate",       gate_base(41),  pwm2_clk_root,           tree };
	Gate pwm3_gate       { "pwm3_gate",       gate_base(42),  pwm3_clk_root,           tree };
	Gate pwm4_gate       { "pwm4_gate",       gate_base(43),  pwm4_clk_root,           tree };
	Gate qspi_gate       { "qspi_gate",       gate_base(47),  qspi_clk_root,           tree };
	Gate nand_gate       { "nand_gate",       gate_base(48),  nand_clk_root,           tree };
	Gate sai1_gate       { "sai1_gate",       gate_base(51),  sai1_clk_root,           tree };
	Gate sai2_gate       { "sai2_gate",       gate_base(52),  sai2_clk_root,           tree };
	Gate sai3_gate       { "sai3_gate",       gate_base(53),  sai3_clk_root,           tree };
	Gate sai4_gate       { "sai4_gate",       gate_base(54),  sai4_clk_root,           tree };
	Gate sai5_gate       { "sai5_gate",       gate_base(55),  sai5_clk_root,           tree };
	Gate sai6_gate       { "sai6_gate",       gate_base(56),  sai6_clk_root,           tree };
	Gate sdma1_gate      { "sdma1_gate",      gate_base(58),  ipg_clk_root,            tree };
	Gate sdma2_gate      { "sdma2_gate",      gate_base(59),  ipg_audio_clk_root,      tree };
	Gate uart1_gate      { "uart1_gate",      gate_base(73),  uart1_clk_root,          tree };
	Gate uart2_gate      { "uart2_gate",      gate_base(74),  uart2_clk_root,          tree };
	Gate uart3_gate      { "uart3_gate",      gate_base(75),  uart3_clk_root,          tree };
	Gate uart4_gate      { "uart4_gate",      gate_base(76),  uart4_clk_root,          tree };
	Gate usb_ctrl1_gate  { "usb_ctrl1_gate",  gate_base(77),  usb_core_ref_clk_root,   tree };
	Gate usb_ctrl2_gate  { "usb_ctrl2_gate",  gate_base(78),  usb_core_ref_clk_root,   tree };
	Gate usb_phy1_gate   { "usb_phy1_gate",   gate_base(79),  usb_phy_ref_clk_root,    tree };
	Gate usb_phy2_gate   { "usb_phy2_gate",   gate_base(80),  usb_phy_ref_clk_root,    tree };
	Gate usdhc1_gate     { "usdhc1_gate",     gate_base(81),  usdhc1_clk_root,         tree };
	Gate usdhc2_gate     { "usdhc2_gate",     gate_base(82),  usdhc2_clk_root,         tree };
	Gate wdog1_gate      { "wdog1_gate",      gate_base(83),  wdog_clk_root,           tree };
	Gate wdog2_gate      { "wdog2_gate",      gate_base(84),  wdog_clk_root,           tree };
	Gate wdog3_gate      { "wdog3_gate",      gate_base(85),  wdog_clk_root,           tree };
	Gate va53_gate       { "va53_gate",       gate_base(86),  vpu_g1_clk_root,         tree };
	Gate gpu_gate        { "gpu_gate",        gate_base(87),  gpu_core_clk_root,       tree };
	Gate vp9_gate        { "vp9_gate",        gate_base(90),  vpu_g2_clk_root,         tree };
	Gate display_gate    { "display_gate",    gate_base(93),  display_dc8000_clk_root, tree };
	Gate tempsensor_gate { "tempsensor_gate", gate_base(98),  ipg_clk_root,            tree };
	Gate vpu_dec_gate    { "vpu_dec_gate",    gate_base(99),  vpu_bus_clk_root,        tree };
	Gate pcie2_gate      { "pcie2_gate",      gate_base(100), pcie2_ctrl_clk_root,     tree };
	Gate mipi_csi1_gate  { "mipi_csi1_gate",  gate_base(101), mipi_csi1_core_clk_root, tree };
	Gate mipi_csi2_gate  { "mipi_csi2_gate",  gate_base(102), mipi_csi2_core_clk_root, tree };
};
