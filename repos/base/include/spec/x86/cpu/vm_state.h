/*
 * \brief   CPU context of a virtual machine for x86
 * \author  Alexander Boettcher
 * \date    2018-10-09
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__VM_STATE_H_
#define _INCLUDE__SPEC__X86__CPU__VM_STATE_H_

#include <base/stdint.h>

namespace Genode
{
	struct Vm_state;
}

struct Genode::Vm_state
{
	template <typename T>
	class Register
	{
		private:

			bool _valid;
			T    _value { };

		public:

			Register() : _valid(false) { }
			Register(T value) : _valid(true), _value(value) { }

			T value() const { return _value; }
			void value(T value) { _value = value; _valid = true; }

			bool valid() const { return _valid; }
			void invalid() { _valid = false; }

			Register &operator = (Register const &other)
			{
				_valid = other._valid;
				/* keep original _value if other._valid is not valid */
				if (_valid)
					_value = other._value;

				return *this;
			}
	};

	struct Range {
		addr_t   base;
		uint32_t limit;
	};

	struct Segment {
		uint16_t sel, ar;
		uint32_t limit;
		addr_t   base;
	};

	Register<addr_t> ax;
	Register<addr_t> cx;
	Register<addr_t> dx;
	Register<addr_t> bx;

	Register<addr_t> bp;
	Register<addr_t> si;
	Register<addr_t> di;

	Register<addr_t> sp;
	Register<addr_t> ip;
	Register<addr_t> ip_len;
	Register<addr_t> flags;

	Register<Segment> es;
	Register<Segment> ds;
	Register<Segment> fs;
	Register<Segment> gs;
	Register<Segment> cs;
	Register<Segment> ss;
	Register<Segment> tr;
	Register<Segment> ldtr;

	Register<Range> gdtr;
	Register<Range> idtr;

	Register<addr_t> cr0;
	Register<addr_t> cr2;
	Register<addr_t> cr3;
	Register<addr_t> cr4;

	Register<addr_t> dr7;

	Register<addr_t> sysenter_ip;
	Register<addr_t> sysenter_sp;
	Register<addr_t> sysenter_cs;

	Register<uint64_t> qual_primary;
	Register<uint64_t> qual_secondary;

	Register<uint32_t> ctrl_primary;
	Register<uint32_t> ctrl_secondary;

	Register<uint32_t> inj_info;
	Register<uint32_t> inj_error;

	Register<uint32_t> intr_state;
	Register<uint32_t> actv_state;

	Register<uint64_t> tsc;
	Register<uint64_t> tsc_offset;

	Register<addr_t>   efer;

	Register<uint64_t> pdpte_0;
	Register<uint64_t> pdpte_1;
	Register<uint64_t> pdpte_2;
	Register<uint64_t> pdpte_3;

	Register<uint64_t> r8;
	Register<uint64_t> r9;
	Register<uint64_t> r10;
	Register<uint64_t> r11;
	Register<uint64_t> r12;
	Register<uint64_t> r13;
	Register<uint64_t> r14;
	Register<uint64_t> r15;

	Register<uint64_t> star;
	Register<uint64_t> lstar;
	Register<uint64_t> fmask;
	Register<uint64_t> kernel_gs_base;

	Register<uint32_t> tpr;
	Register<uint32_t> tpr_threshold;

	unsigned exit_reason;

	class Fpu {
		private :

			uint8_t _value[512] { };
			bool    _valid      { false };

		public:

			bool valid() const { return _valid; }
			void invalid() { _valid = false; }

			template <typename FUNC>
			void value(FUNC const &fn) {
				_valid = true;
				fn(_value, sizeof(_value));
			};

			Fpu &operator = (Fpu const &)
			{
				_valid = false;
				return *this;
			}
	} fpu __attribute__((aligned(16)));
};

#endif /* _INCLUDE__SPEC__X86__CPU__VM_STATE_H_ */
