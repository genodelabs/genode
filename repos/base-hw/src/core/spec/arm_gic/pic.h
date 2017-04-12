/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_GIC__PIC_H_
#define _CORE__SPEC__ARM_GIC__PIC_H_

#include <hw/spec/arm/pic.h>

namespace Genode { class Pic; }
namespace Kernel { using Pic = Genode::Pic; }

class Genode::Pic : public Hw::Pic
{
	public:

		enum { IPI = 1 };

		/**
		 * Raise inter-processor IRQ of the CPU with kernel name 'cpu_id'
		 */
		void send_ipi(unsigned const cpu_id)
		{
			using Sgir = Distributor::Sgir;
			Sgir::access_t sgir = 0;
			Sgir::Sgi_int_id::set(sgir, IPI);
			Sgir::Cpu_target_list::set(sgir, 1 << cpu_id);
			_distr.write<Sgir>(sgir);
		}

		/**
		 * Raise inter-processor interrupt on all other cores
		 */
		void send_ipi()
		{
			using Sgir = Distributor::Sgir;
			Sgir::access_t sgir = 0;
			Sgir::Sgi_int_id::set(sgir, IPI);
			Sgir::Target_list_filter::set(sgir,
			                              Sgir::Target_list_filter::ALL_OTHER);
			_distr.write<Sgir>(sgir);
		}
};

#endif /* _CORE__SPEC__ARM_GIC__PIC_H_ */
