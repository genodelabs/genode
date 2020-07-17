/*
 * \brief  Global power controller for i.MX8
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

#include <base/env.h>
#include <util/string.h>

struct Gpc
{
	enum Pu {
		MIPI      = 0,
		PCIE_1    = 1,
		USB_OTG_1 = 2,
		USB_OTG_2 = 3,
		GPU       = 4,
		VPU       = 5,
		HDMI      = 6,
		DISP      = 7,
		CSI_1     = 8,
		CSI_2     = 9,
		PCIE_2    = 10,
		INVALID,
	};

	enum {
		SIP_SERVICE_FUNC = 0xc2000000,
		GPC_PM_DOMAIN    = 0x3,
		ON               = 1,
		OFF              = 0
	};

	Genode::Env & env;

	Pu pu(Genode::String<64> name)
	{
		if (name == "mipi")      { return MIPI;      }
		if (name == "pcie_1")    { return PCIE_1;    }
		if (name == "usb_otg_1") { return USB_OTG_1; }
		if (name == "usb_otg_2") { return USB_OTG_2; }
		if (name == "gpu")       { return GPU;       }
		if (name == "vpu")       { return VPU;       }
		if (name == "hdmi")      { return HDMI;      }
		if (name == "disp")      { return DISP;      }
		if (name == "csi_1")     { return CSI_1;     }
		if (name == "csi_2")     { return CSI_2;     }
		if (name == "pcie_2")    { return PCIE_2;    }
		return INVALID;
	}

	void enable(Genode::String<64> name)
	{
		Genode::Pd_session::Managing_system_state state;
		state.r[0] = SIP_SERVICE_FUNC;
		state.r[1] = GPC_PM_DOMAIN;
		state.r[2] = pu(name);
		state.r[3] = ON;

		if (state.r[2] == INVALID) {
			Genode::warning("Power domain ", name.string(), " is not valid!");
			return;
		}

		env.pd().managing_system(state);
	}

	void disable(Genode::String<64> name)
	{
		Genode::Pd_session::Managing_system_state state;
		state.r[0] = SIP_SERVICE_FUNC;
		state.r[1] = GPC_PM_DOMAIN;
		state.r[2] = pu(name);
		state.r[3] = OFF;

		if (state.r[2] == INVALID) {
			Genode::warning("Power domain ", name.string(), " is not valid!");
			return;
		}

		env.pd().managing_system(state);
	}

	Gpc(Genode::Env & env) : env(env)
	{
		for (unsigned domain = MIPI; domain <= PCIE_2; domain++) {
			Genode::Pd_session::Managing_system_state state;
			state.r[0] = SIP_SERVICE_FUNC;
			state.r[1] = GPC_PM_DOMAIN;
			state.r[2] = domain;
			state.r[3] = OFF;
		}
	};
};
