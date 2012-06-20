/*
 * \brief  Syscall bindings for the NOVA microhypervisor x86_64
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2012-06-06
 */

/*
 * Copyright (c) 2012 Genode Labs
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
	inline mword_t rdi(Syscall s, uint8_t flags, mword_t sel)
	{
		return sel << 8 | (flags & 0xf) << 4 | s;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_0(Syscall s, uint8_t flags, mword_t sel = 0)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status)
			      :
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_1(Syscall s, uint8_t flags, mword_t p1, mword_t * p2 = 0)
	{
		mword_t status = rdi(s, flags, 0);

		asm volatile ("syscall"
		              : "+D" (status), "+S" (p1)
			      : 
		              : "rcx", "r11", "memory");
		if (p2) *p2 = p1;
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_2(Syscall s, uint8_t flags, mword_t sel, mword_t p1, mword_t p2)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status)
		              : "S" (p1), "d" (p2)
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_3(Syscall s, uint8_t flags, unsigned sel,
	                         mword_t p1, mword_t p2, mword_t p3)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status)
		              : "S" (p1), "d" (p2), "a" (p3)
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_4(Syscall s, uint8_t flags, mword_t sel,
	                         mword_t p1, mword_t p2, mword_t p3, mword_t p4)
	{
		mword_t status = rdi(s, flags, sel);
		register mword_t r8 asm ("r8") = p4;

		asm volatile ("syscall;"
		              : "+D" (status)
		              : "S" (p1), "d" (p2), "a" (p3), "r" (r8)
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_5(Syscall s, uint8_t flags, mword_t sel,
	                         mword_t p1, mword_t p2, mword_t &p3, mword_t &p4)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status), "+S"(p3), "+d"(p4)
			      :
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t call(mword_t pt)
	{
		return syscall_0(NOVA_CALL, 0, pt);
	}


	ALWAYS_INLINE
	inline void reply(void *next_sp)
	{
		asm volatile ("mov %1, %%rsp;"
		              "syscall;"
		              :
		              : "D" (NOVA_REPLY), "ir" (next_sp)
		              : "memory");
	}


	ALWAYS_INLINE
	inline uint8_t create_pd(mword_t pd0, mword_t pd, Crd crd)
	{
		return syscall_2(NOVA_CREATE_PD, 0, pd0, pd, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t create_ec(mword_t ec, mword_t pd,
	                         mword_t cpu, mword_t utcb,
	                         mword_t esp, mword_t evt,
	                         bool global = 0)
	{
		return syscall_4(NOVA_CREATE_EC, global, ec, pd,
		                 (cpu & 0xfff) | (utcb & ~0xfff),
		                 esp, evt);
	}


	ALWAYS_INLINE
	inline uint8_t ec_ctrl(mword_t ec)
	{
		return syscall_0(NOVA_EC_CTRL, 0, ec);
	}

	ALWAYS_INLINE
	inline uint8_t create_sc(mword_t sc, mword_t pd, mword_t ec, Qpd qpd)
	{
		return syscall_3(NOVA_CREATE_SC, 0, sc, pd, ec, qpd.value());
	}


	ALWAYS_INLINE
	inline uint8_t create_pt(mword_t pt, mword_t pd, mword_t ec, Mtd mtd, mword_t rip)
	{
		return syscall_4(NOVA_CREATE_PT, 0, pt, pd, ec, mtd.value(), rip);
	}


	ALWAYS_INLINE
	inline uint8_t create_sm(mword_t sm, mword_t pd, mword_t cnt)
	{
		return syscall_2(NOVA_CREATE_SM, 0, sm, pd, cnt);
	}


	ALWAYS_INLINE
	inline uint8_t revoke(Crd crd, bool self = true)
	{
		return syscall_1(NOVA_REVOKE, self, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t lookup(Crd &crd)
	{
		mword_t crd_r;
		uint8_t res=syscall_1(NOVA_LOOKUP, 0, crd.value(), &crd_r);
		crd = Crd(crd_r);
		return res;
	}

	/**
	 * Semaphore operations
	 */
	enum Sem_op { SEMAPHORE_UP = 0, SEMAPHORE_DOWN = 1 };


	ALWAYS_INLINE
	inline uint8_t sm_ctrl(mword_t sm, Sem_op op)
	{
		return syscall_0(NOVA_SM_CTRL, op, sm);
	}


	ALWAYS_INLINE
	inline uint8_t sc_ctrl(mword_t sm, Sem_op op, mword_t &time)
	{
		mword_t status = rdi(NOVA_SC_CTRL, op, sm);
		mword_t time_h;

		uint8_t res = syscall_5(NOVA_SC_CTRL, op, sm, 0, 0, time_h, time);
		asm volatile ("syscall"
		              : "+D" (status), "=S"(time_h), "=d"(time)
			      :
		              : "rcx", "r11", "memory");
		
		time = (time_h & ~0xFFFFFFFFULL) | (time & 0xFFFFFFFFULL);
		return res;
	}


	ALWAYS_INLINE
	inline uint8_t assign_pci(mword_t pd, mword_t mem, mword_t rid)
	{
		return syscall_2(NOVA_ASSIGN_GSI, 0, pd, mem, rid);
	}

	ALWAYS_INLINE
	inline uint8_t assign_gsi(mword_t sm, mword_t dev, mword_t rid)
	{
		mword_t dummy1, dummy2;
		return syscall_5(NOVA_ASSIGN_GSI, 0, sm, dev, rid, dummy1, dummy2);
	}

	ALWAYS_INLINE
	inline uint8_t assign_gsi(mword_t sm, mword_t dev, mword_t rid, mword_t &msi_addr, mword_t &msi_data)
	{
		return syscall_5(NOVA_ASSIGN_GSI, 0, sm, dev, rid, msi_addr, msi_data);
	}
}
#endif /* _PLATFORM__NOVA_SYSCALLS_H_ */
