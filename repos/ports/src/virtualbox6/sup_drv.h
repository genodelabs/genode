/*
 * \brief  Suplib driver implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2020-10-12
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SUP_DRV_H_
#define _SUP_DRV_H_

/* local includes */
#include <sup.h>
#include <sup_gip.h>
#include <sup_gmm.h>
#include <sup_vcpu.h>

/* Genode includes */
#include <base/env.h>
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>

namespace Sup { struct Drv; }

namespace Pthread { struct Emt; }

class Sup::Drv
{
	public:

		enum class Cpu_virt { NONE, VMX, SVM };

		class Virtualization_support_missing : Exception { };

	private:

		Env &_env;

		Attached_rom_dataspace const _platform_info_rom { _env, "platform_info" };

		Affinity::Space const _affinity_space { _env.cpu().affinity_space() };

		Cpu_count const _cpu_count { _affinity_space.total() };

		Cpu_freq_khz _cpu_freq_khz_from_rom();

		Cpu_virt _cpu_virt_from_rom();

		Cpu_virt const _cpu_virt { _cpu_virt_from_rom() };

		Vm_connection _vm_connection { _env, "",
		                               Cpu_session::PRIORITY_LIMIT / 2 };

		Gip _gip { _env, _cpu_count, _cpu_freq_khz_from_rom() };
		Gmm _gmm { _env, _vm_connection };

	public:

		Drv(Env &env) : _env(env) { }

		SUPGLOBALINFOPAGE *gip() { return _gip.gip(); }

		Gmm &gmm() { return _gmm; }

		Cpu_virt cpu_virt() { return _cpu_virt; }

		/*
		 * \throw Virtualization_support_missing
		 */
		Vcpu & create_vcpu(VM &, Cpu_index, Pthread::Emt &);
};

#endif /* _SUP_DRV_H_ */
