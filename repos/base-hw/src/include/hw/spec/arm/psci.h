/**
 * \brief  Power State Coordination Interface 1.0
 * \author Piotr Tworek
 * \date   2019-11-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INCLUDE__HW__SPEC__ARM__PSCI_H_
#define _SRC__INCLUDE__HW__SPEC__ARM__PSCI_H_

namespace Hw { template <typename CONDUIT_FUNCTOR> class Psci; }

template <typename CONDUIT_FUNCTOR>
class Hw::Psci
{
	private:

		enum {
			PSCI_32_BASE = 0x84000000,
			PSCI_64_BASE = 0xC4000000,
		};

		enum {
			CPU_OFF = 2,
			CPU_ON  = 3,
		};

		static inline constexpr bool _arch_32() {
			return sizeof(unsigned long) == 4; }

		static inline constexpr unsigned long _psci_func(int func,
		                                                 bool only_32)
		{
			return ((only_32 || _arch_32()) ? PSCI_32_BASE
			                                : PSCI_64_BASE) + func;
		}

	public:

		static bool cpu_on(unsigned long cpu_id, void *entrypoint)
		{
			return CONDUIT_FUNCTOR::call(_psci_func(CPU_ON, false),
			                             cpu_id,
			                             (unsigned long)entrypoint,
			                             cpu_id) == 0;
		}

		static bool cpu_off()
		{
			return CONDUIT_FUNCTOR::call(_psci_func(CPU_OFF, true),
			                             0, 0, 0) == 0;
		}
};

#endif /* _SRC__INCLUDE__HW__SPEC__ARM__PSCI_H_ */
