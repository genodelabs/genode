/*
 * \brief  Syscall bindings for the NOVA microhypervisor
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-12-27
 */

/*
 * Copyright (c) 2009 Genode Labs
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _PLATFORM__NOVA_SYSCALLS_H_
#define _PLATFORM__NOVA_SYSCALLS_H_

#include <nova/stdint.h>
#include <nova/syscall-generic.h>

#define ALWAYS_INLINE __attribute__((always_inline))

namespace Nova {

	ALWAYS_INLINE
	inline unsigned eax(Syscall s, uint8_t flags, unsigned sel)
	{
		return sel << 8 | (flags & 0xf) << 4 | s;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_0(Syscall s, uint8_t flags, unsigned sel = 0)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              :
		              : "ecx", "edx", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_1(Syscall s, uint8_t flags, unsigned sel, mword_t p1)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              : "D" (p1)
		              : "ecx", "edx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_2(Syscall s, uint8_t flags, unsigned sel, mword_t p1, mword_t p2)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              : "D" (p1), "S" (p2)
		              : "ecx", "edx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_3(Syscall s, uint8_t flags, unsigned sel,
	                         mword_t p1, mword_t p2, mword_t p3)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  push %%ebx;"
		              "  mov  %%edx, %%ebx;"
		              "  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              " pop %%ebx;"
		              : "+a" (status)
		              : "D" (p1), "S" (p2), "d" (p3)
		              : "ecx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_4(Syscall s, uint8_t flags, unsigned sel,
	                         mword_t p1, mword_t p2, mword_t p3, mword_t p4)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  push %%ebp;"
		              "  push %%ebx;"

		              "  mov %%ecx, %%ebx;"
		              "  mov %%esp, %%ecx;"
		              "  mov %%edx, %%ebp;"

		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "sysenter;"
		              "1:"

		              "  pop %%ebx;"
		              "  pop %%ebp;"
		              : "+a" (status)
		              : "D" (p1), "S" (p2), "c" (p3), "d" (p4)
		              : "memory");
		return status;
	}

	ALWAYS_INLINE
	inline uint8_t syscall_5(Syscall s, uint8_t flags, mword_t sel,
	                         mword_t &p1, mword_t &p2)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  movl %%esp, %%ecx;"
		              "  movl $1f, %%edx;"
		              "sysenter;"
		              "1:"
		              : "+a" (status), "+D" (p1), "+S" (p2)
		              : 
		              : "ecx", "edx", "memory");
		return status;
	}

	ALWAYS_INLINE
	inline uint8_t call(unsigned pt)
	{
		return syscall_0(NOVA_CALL, 0, pt);
	}


	ALWAYS_INLINE
	__attribute__((noreturn))
	inline void reply(void *next_sp)
	{
		asm volatile ("sysenter;"
		              :
		              : "a" (NOVA_REPLY), "c" (next_sp)
		              : "memory");
		__builtin_unreachable();
	}


	ALWAYS_INLINE
	inline uint8_t create_pd(unsigned pd0, unsigned pd, Crd crd)
	{
		return syscall_2(NOVA_CREATE_PD, 0, pd0, pd, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t create_ec(unsigned ec, unsigned pd,
	                         mword_t cpu, mword_t utcb,
	                         mword_t esp, mword_t evt,
	                         bool global = 0)
	{
		return syscall_4(NOVA_CREATE_EC, global, ec, pd,
		                 (cpu & 0xfff) | (utcb & ~0xfff),
		                 esp, evt);
	}


	ALWAYS_INLINE
	inline uint8_t ec_ctrl(unsigned ec)
	{
		return syscall_0(NOVA_EC_CTRL, 0, ec);
	}


	ALWAYS_INLINE
	inline uint8_t create_sc(unsigned sc, unsigned pd, unsigned ec, Qpd qpd)
	{
		return syscall_3(NOVA_CREATE_SC, 0, sc, pd, ec, qpd.value());
	}


	ALWAYS_INLINE
	inline uint8_t pt_ctrl(mword_t pt, mword_t pt_id)
	{
		return syscall_1(NOVA_PT_CTRL, 0, pt, pt_id);
	}


	ALWAYS_INLINE
	inline uint8_t create_pt(unsigned pt, unsigned pd, unsigned ec, Mtd mtd,
	                         mword_t eip, bool id_equal_pt = true)
	{
		uint8_t res = syscall_4(NOVA_CREATE_PT, 0, pt, pd, ec, mtd.value(), eip);

		if (!id_equal_pt || res != NOVA_OK)
			return res;

		return pt_ctrl(pt, pt);
	}


	ALWAYS_INLINE
	inline uint8_t create_sm(unsigned sm, unsigned pd, mword_t cnt)
	{
		return syscall_2(NOVA_CREATE_SM, 0, sm, pd, cnt);
	}


	ALWAYS_INLINE
	inline uint8_t revoke(Crd crd, bool self = true)
	{
		return syscall_1(NOVA_REVOKE, self, 0, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t lookup(Crd &crd)
	{
		mword_t status = eax(NOVA_LOOKUP, 0, 0);
		mword_t raw = crd.value();

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status), "+D" (raw)
		              :
		              : "ecx", "edx", "memory");

		crd = Crd(raw);
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t sm_ctrl(unsigned sm, Sem_op op)
	{
		return syscall_0(NOVA_SM_CTRL, op, sm);
	}


	ALWAYS_INLINE
	inline uint8_t assign_pci(mword_t pd, mword_t mem, mword_t rid)
	{
		return syscall_2(NOVA_ASSIGN_PCI, 0, pd, mem, rid);
	}

	ALWAYS_INLINE
	inline uint8_t assign_gsi(mword_t sm, mword_t dev, mword_t cpu, mword_t &msi_addr, mword_t &msi_data)
	{
		msi_addr = dev;
		msi_data = cpu;

		return syscall_5(NOVA_ASSIGN_GSI, 0, sm, msi_addr, msi_data);
	}
}
#endif /* _PLATFORM__NOVA_SYSCALLS_H_ */
