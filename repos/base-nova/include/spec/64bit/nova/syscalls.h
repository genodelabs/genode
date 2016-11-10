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

#ifndef _INCLUDE__SPEC__64BIT__NOVA__SYSCALLS_H_
#define _INCLUDE__SPEC__64BIT__NOVA__SYSCALLS_H_

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
	inline uint8_t syscall_1(Syscall s, uint8_t flags, mword_t sel, mword_t p1,
	                         mword_t * p2 = 0)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status), "+S" (p1)
		              :
		              : "rcx", "r11", "memory");
		if (p2) *p2 = p1;
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_2(Syscall s, uint8_t flags, mword_t sel, mword_t p1,
	                         mword_t p2)
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
	                         mword_t &p1, mword_t &p2, mword_t p3 = ~0UL)
	{
		mword_t status = rdi(s, flags, sel);

		asm volatile ("syscall"
		              : "+D" (status), "+S"(p1), "+d"(p2)
		              : "a" (p3)
		              : "rcx", "r11", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t call(mword_t pt)
	{
		return syscall_1(NOVA_CALL, 0, pt, 0);
	}


	ALWAYS_INLINE
	__attribute__((noreturn))
	inline void reply(void *next_sp, unsigned long sm = 0)
	{
		mword_t syscall = rdi(NOVA_REPLY, 0, sm);

		asm volatile ("mov %1, %%rsp;"
		              "syscall;"
		              :
		              : "D" (syscall), "ir" (next_sp)
		              : "memory");
		__builtin_unreachable();
	}


	ALWAYS_INLINE
	inline uint8_t create_pd(mword_t pd0, mword_t pd, Crd crd,
	                         unsigned lower_limit, unsigned long upper_limit)
	{
		return syscall_3(NOVA_CREATE_PD, 0, pd0, pd, crd.value(),
		                 upper_limit << 32 | lower_limit);
	}


	/**
	 * Create an EC.
	 *
	 * \param ec     two selectors - ec && ec + 1
	 *               First selector must be unused and second selector is
	 *               either unused or must be a valid portal selector.
	 *               The thread will call this portal if the PD it runs in runs
	 *               out of kernel memory.
	 * \param pd     selector of PD the EC will created in
	 * \param cpu    CPU number the EC will run on
	 * \param utcb   PD local address where the UTCB of the EC will be appear
	 * \param esp    initial stack address
	 * \param evt    base selector for all exception portals of the EC
	 * \param global if true  - thread requires a SC to be runnable
	 *               if false - thread is runnable solely if it receives a IPC
	 *                          (worker thread)
	 */
	ALWAYS_INLINE
	inline uint8_t create_ec(mword_t ec, mword_t pd, mword_t cpu, mword_t utcb,
	                         mword_t esp, mword_t evt, bool global = false)
	{
		return syscall_4(NOVA_CREATE_EC, global, ec, pd,
		                 (cpu & 0xfff) | (utcb & ~0xfff),
		                 esp, evt);
	}


	ALWAYS_INLINE
	inline uint8_t ec_ctrl(Ec_op op, mword_t ec = ~0UL, mword_t para = ~0UL)
	{
		return syscall_1(NOVA_EC_CTRL, op, ec, para);
	}


	ALWAYS_INLINE
	inline uint8_t create_sc(mword_t sc, mword_t pd, mword_t ec, Qpd qpd)
	{
		return syscall_3(NOVA_CREATE_SC, 0, sc, pd, ec, qpd.value());
	}


	ALWAYS_INLINE
	inline uint8_t pt_ctrl(mword_t pt, mword_t pt_id)
	{
		return syscall_1(NOVA_PT_CTRL, 0, pt, pt_id);
	}


	ALWAYS_INLINE
	inline uint8_t create_pt(mword_t pt, mword_t pd, mword_t ec, Mtd mtd,
	                         mword_t rip, bool id_equal_pt = true)
	{
		uint8_t res = syscall_4(NOVA_CREATE_PT, 0, pt, pd, ec, mtd.value(), rip);

		if (!id_equal_pt || res != NOVA_OK)
			return res;

		return pt_ctrl(pt, pt);
	}


	ALWAYS_INLINE
	inline uint8_t create_sm(mword_t sm, mword_t pd, mword_t cnt)
	{
		return syscall_3(NOVA_CREATE_SM, 0, sm, pd, cnt, 0);
	}


	ALWAYS_INLINE
	inline uint8_t create_si(mword_t si, mword_t pd, mword_t value, mword_t sm)
	{
		return syscall_3(NOVA_CREATE_SM, 0, si, pd, value, sm);
	}


	/**
	 * Revoke memory, capabilities or i/o ports from a PD
	 *
	 * \param crd    describes region and type of resource
	 * \param self   also revoke from source PD iif self == true
	 * \param remote if true the 'pd' parameter below is used, otherwise
	 *               current PD is used as source PD
	 * \param pd     selector describing remote PD
	 * \param sm     SM selector which gets an up() by the kernel if the
	 *               memory of the current revoke invocation gets freed up
	 *               (end of RCU period)
	 * \param kim    keep_in_mdb - if set to true the kernel will make the
	 *               resource inaccessible solely inside the specified pd.
	 *               All already beforehand delegated resources will not be
	 *               changed, e.g. revoked. All rights of the local resource
	 *               will be removed (independent of what is specified by crd).
	 */
	ALWAYS_INLINE
	inline uint8_t revoke(Crd crd, bool self = true, bool remote = false,
	                      mword_t pd = 0, mword_t sm = 0, bool kim = false)
	{
		uint8_t flags = self ? 0x1 : 0;

		if (remote)
			flags |= 0x2;

		if (kim)
			flags |= 0x4;

		mword_t value_crd = crd.value();
		return syscall_5(NOVA_REVOKE, flags, sm, value_crd, pd);
	}


	/*
	 * Shortcut for revoke, where solely the local cap should be revoked and
	 * not all subsequent delegations of the local cap.
	 */
	ALWAYS_INLINE
	inline uint8_t drop(Crd crd) {
		return revoke(crd, true, false, 0, 0, true); }


	ALWAYS_INLINE
	inline uint8_t lookup(Crd &crd)
	{
		mword_t crd_r;
		uint8_t res = syscall_1(NOVA_LOOKUP, 0, 0, crd.value(), &crd_r);
		crd = Crd(crd_r);
		return res;
	}


	ALWAYS_INLINE
	inline uint8_t delegate(mword_t pd_snd, mword_t pd_dst, Crd crd_dst)
	{
		return syscall_2(NOVA_LOOKUP, 1, pd_snd, crd_dst.value(), pd_dst);
	}


	ALWAYS_INLINE
	inline uint8_t sm_ctrl(mword_t sm, Sem_op op, unsigned long long timeout = 0)
	{
		return syscall_2(NOVA_SM_CTRL, op, sm, timeout >> 32,
		                 timeout & 0xFFFFFFFFULL);
	}


	ALWAYS_INLINE
	inline uint8_t si_ctrl(mword_t sm, Sem_op op, mword_t &value, mword_t &cnt)
	{
		return syscall_5(NOVA_SM_CTRL, op, sm, value, cnt);
	}


	ALWAYS_INLINE
	inline uint8_t sc_ctrl(mword_t sm, unsigned long long &time)
	{
		mword_t time_h = 0, time_l = 0;
		uint8_t res = syscall_5(NOVA_SC_CTRL, 0, sm, time_h, time_l);
		time = time_h;
		time = (time << 32ULL) | (time_l & 0xFFFFFFFFULL);
		return res;
	}


	ALWAYS_INLINE
	inline uint8_t pd_ctrl(mword_t pd_src, Pd_op op, mword_t pd_dst, mword_t transfer)
	{
		return syscall_5(NOVA_PD_CTRL, op, pd_src, pd_dst, transfer);
	}


	ALWAYS_INLINE
	inline uint8_t pd_ctrl_debug(mword_t pd, mword_t &limit, mword_t &usage)
	{
		return syscall_5(NOVA_PD_CTRL, Pd_op::PD_DEBUG, pd, limit, usage);
	}


	ALWAYS_INLINE
	inline uint8_t assign_pci(mword_t pd, mword_t mem, mword_t rid)
	{
		return syscall_2(NOVA_ASSIGN_PCI, 0, pd, mem, rid);
	}


	ALWAYS_INLINE
	inline uint8_t assign_gsi(mword_t sm, mword_t dev, mword_t cpu,
	                          mword_t &msi_addr, mword_t &msi_data,
	                          mword_t si = ~0UL)
	{
		msi_addr = dev;
		msi_data = cpu;
		return syscall_5(NOVA_ASSIGN_GSI, 0, sm, msi_addr, msi_data, si);
	}
}
#endif /* _INCLUDE__SPEC__64BIT__NOVA__SYSCALLS_H_ */
