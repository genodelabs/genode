/*
 * \brief  System reset controller for i.MX8
 * \author Stefan Kalkowski
 * \date   2021-05-21
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/env.h>
#include <base/log.h>
#include <os/attached_mmio.h>
#include <util/string.h>

struct Src : Genode::Attached_mmio
{
	struct Mipi_phy : Register<0x28, 32>
	{
		struct Byte  : Bitfield<1, 1> {};
		struct Reset : Bitfield<2, 1> {};
		struct Dpi   : Bitfield<3, 1> {};
		struct Esc   : Bitfield<4, 1> {};
		struct Pclk  : Bitfield<5, 1> {};
	};

	void enable(Genode::String<64> name)
	{
		if (name == "mipi_dsi_byte") { write<Mipi_phy::Byte>(1); return; }
		if (name == "mipi_dsi_dpi")  { write<Mipi_phy::Dpi>(1);  return; }
		if (name == "mipi_dsi_esc")  { write<Mipi_phy::Esc>(1);  return; }
		if (name == "mipi_dsi_pclk") { write<Mipi_phy::Pclk>(1); return; }
		warning("Reset domain ", name, " is unknown!");
	}

	void disable(Genode::String<64> name)
	{
		if (name == "mipi_dsi_byte") { write<Mipi_phy::Byte>(0); return; }
		if (name == "mipi_dsi_dpi")  { write<Mipi_phy::Dpi>(0);  return; }
		if (name == "mipi_dsi_esc")  { write<Mipi_phy::Esc>(0);  return; }
		if (name == "mipi_dsi_pclk") { write<Mipi_phy::Pclk>(0); return; }
		warning("Reset domain ", name, " is unknown!");
	}

	enum {
		SRC_MMIO_BASE = 0x30390000,
		SRC_MMIO_SIZE = 0x10000,
	};

	Src(Genode::Env & env)
	: Attached_mmio(env, SRC_MMIO_BASE, SRC_MMIO_SIZE) { };
};

