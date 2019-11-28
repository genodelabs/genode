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

#pragma once

#include <psci_call.h>

namespace Hw {

	enum class Psci_conduit { HVC, SVC };

	enum {
		PSCI_32 = 32,
		PSCI_64 = 64,
	};

	template <Psci_conduit CONDUIT, unsigned SIZE>
	class Psci {

		private:
			enum {
				PSCI_32_BASE = 0x84000000,
				PSCI_64_BASE = 0xC4000000,
			};

			enum {
				CPU_OFF = 2,
				CPU_ON  = 3,
			};

			PSCI_CALL_IMPL(hvc_call, hvc)
			PSCI_CALL_IMPL(svc_call, svc)

			static inline int constexpr psci_call(unsigned long func, unsigned long a0,
			                                      unsigned long a1, unsigned long a2)
			{
				if (CONDUIT == Psci_conduit::HVC)
					return hvc_call(func, a0, a1, a2);
				return svc_call(func, a0, a1, a2);
			}

			static inline constexpr unsigned long psci_func(int func, bool only_32)
			{
				if (SIZE == PSCI_64 && !only_32)
					return PSCI_64_BASE + func;
				return PSCI_32_BASE + func;
			}

		public:

			static bool cpu_on(unsigned long cpu_id, void *entrypoint)
			{
				return psci_call(psci_func(CPU_ON, false),
				                 cpu_id,
				                 (unsigned long)entrypoint,
				                 cpu_id) == 0;
			}

			static bool cpu_off() {
				return psci_call(psci_func(CPU_OFF, true), 0, 0, 0) == 0; }

	};
}

#undef PSCI_CALL_IMPL
