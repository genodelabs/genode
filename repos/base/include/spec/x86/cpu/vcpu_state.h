/*
 * \brief   Virtual CPU context for x86
 * \author  Alexander Boettcher
 * \author  Christian Helmuth
 * \author  Benjamin Lamowski
 * \date    2018-10-09
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__VM_STATE_H_
#define _INCLUDE__SPEC__X86__CPU__VM_STATE_H_

#include <base/stdint.h>
#include <util/noncopyable.h>

namespace Genode { struct Vcpu_state; }


/*
 * The state of one virtual CPU (vCPU) as available via the VM session for x86
 *
 * The state object is designed for bidirectional transfer of register state,
 * which means it reflects vCPU state on VM exits but also supports loading
 * updating register state on VM entry. Therefore, each register contains not
 * only the actual register value but also a 'charged' state.
 *
 * The hypervisor charges registers as requested by the VMM on VM exit with the
 * current virtual CPU state. The VMM for its part charges registers it intends
 * to update with new values before VM entry (e.g., after I/O emulation). Both
 * parties are required to 'discharge()' the vCPU state explicitly if registers
 * charged by the other party should not be considered on return. The common
 * case is to discharge all registers, charge some updates and transfer
 * execution to the other party.
 */
class Genode::Vcpu_state
{
	private:

		Vcpu_state & operator = (Vcpu_state const &) = default;

		Vcpu_state(Vcpu_state const&) = delete;

	public:

		Vcpu_state() = default;

		template <typename T>
		class Register : Noncopyable
		{
			private:

				friend class Vcpu_state;

				T    _value   { };
				bool _charged { false };

				/*
				 * Trick used by Vcpu_state::discharge() to discharge all
				 * registers at once. Note, the register value is kept intact.
				 */
				Register & operator = (Register const &)
				{
					_charged = false;

					return *this;
				}

			public:

				bool charged() const { return _charged; }

				T value() const { return _value; }

				void charge(T const &value)
				{
					_charged = true;
					_value   = value;
				}

				/*
				 * Charge without changing value
				 *
				 * \noapi
				 */
				void set_charged() { _charged = true; }

				/*
				 * Update value if not yet charged
				 *
				 * \noapi
				 */
				void update(T const &value)
				{
					if (!_charged) {
						_value   = value;
						_charged = true;
					}
				}
		};

		struct Range
		{
			uint32_t limit;
			addr_t   base;
		};

		struct Segment
		{
			uint16_t sel, ar;
			uint32_t limit;
			addr_t   base;
		};

		Register<addr_t> ax { };
		Register<addr_t> cx { };
		Register<addr_t> dx { };
		Register<addr_t> bx { };

		Register<addr_t> bp { };
		Register<addr_t> si { };
		Register<addr_t> di { };

		Register<addr_t> sp { };
		Register<addr_t> ip { };
		Register<addr_t> ip_len { };
		Register<addr_t> flags { };

		Register<Segment> es { };
		Register<Segment> ds { };
		Register<Segment> fs { };
		Register<Segment> gs { };
		Register<Segment> cs { };
		Register<Segment> ss { };
		Register<Segment> tr { };
		Register<Segment> ldtr { };

		Register<Range> gdtr { };
		Register<Range> idtr { };

		Register<addr_t> cr0 { };
		Register<addr_t> cr2 { };
		Register<addr_t> cr3 { };
		Register<addr_t> cr4 { };

		Register<addr_t> dr7 { };

		Register<addr_t> sysenter_ip { };
		Register<addr_t> sysenter_sp { };
		Register<addr_t> sysenter_cs { };

		Register<uint64_t> qual_primary { };
		Register<uint64_t> qual_secondary { };

		Register<uint32_t> ctrl_primary { };
		Register<uint32_t> ctrl_secondary { };

		Register<uint32_t> inj_info { };
		Register<uint32_t> inj_error { };

		Register<uint32_t> intr_state { };
		Register<uint32_t> actv_state { };

		Register<uint64_t> tsc { };
		Register<uint64_t> tsc_offset { };
		Register<uint64_t> tsc_aux { };

		Register<addr_t>   efer { };

		Register<uint64_t> pdpte_0 { };
		Register<uint64_t> pdpte_1 { };
		Register<uint64_t> pdpte_2 { };
		Register<uint64_t> pdpte_3 { };

		Register<uint64_t> r8  { };
		Register<uint64_t> r9  { };
		Register<uint64_t> r10 { };
		Register<uint64_t> r11 { };
		Register<uint64_t> r12 { };
		Register<uint64_t> r13 { };
		Register<uint64_t> r14 { };
		Register<uint64_t> r15 { };

		Register<uint64_t> star  { };
		Register<uint64_t> lstar { };
		Register<uint64_t> cstar { };
		Register<uint64_t> fmask { };
		Register<uint64_t> kernel_gs_base { };

		Register<uint32_t> tpr { };
		Register<uint32_t> tpr_threshold { };

		unsigned exit_reason { };

		class Fpu : Noncopyable
		{
			public:

				struct State
				{
					uint8_t _buffer[512] { };
				} __attribute__((aligned(16)));

			private:

				friend class Vcpu_state;

				State _state   { };
				bool  _charged { false };

				/* see comment for Register::operator=() */
				Fpu & operator = (Fpu const &)
				{
					_charged = false;

					return *this;
				}

			public:

				bool charged() const { return _charged; }

				void with_state(auto const &fn) const { fn(_state); }

				void charge(auto const &fn)
				{
					_charged = true;
					fn(_state);
				}
		};

		Fpu fpu __attribute__((aligned(16))) { };

		/*
		 * Registers transfered by hypervisor from guest on VM exit are charged.
		 * Discharged registers are not loaded into guest on VM entry.
		 */
		void discharge()
		{
			/* invoke operator= for all registers with all charged flags reset */
			*this = Vcpu_state { };
		}
};

#endif /* _INCLUDE__SPEC__X86__CPU__VCPU_STATE_H_ */
