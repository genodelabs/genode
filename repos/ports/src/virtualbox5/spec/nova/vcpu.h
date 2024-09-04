/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__VCPU_H_
#define _VIRTUALBOX__SPEC__NOVA__VCPU_H_

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <cpu/consts.h>
#include <util/flex_iterator.h>
#include <util/touch.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>

#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>
#include <vmm/log.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* VirtualBox includes */
#include "PGMInternal.h" /* enable access to pgm.s.* */

#include "HMInternal.h" /* enable access to hm.s.* */
#include "CPUMInternal.h" /* enable access to cpum.s.* */

#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/err.h>

#include <VBox/vmm/pdmapi.h>

#include <iprt/time.h>

/* Genode's VirtualBox includes */
#include "sup.h"

/* Genode libc pthread binding */
#include <internal/pthread.h>

/* LibC includes */
#include <setjmp.h>

#include <VBox/vmm/rem.h>

static bool debug_map_memory = false;

/*
 * VirtualBox stores segment attributes in Intel format using a 32-bit
 * value. NOVA represents the attributes in packed format using a 16-bit
 * value.
 */
static inline Genode::uint16_t sel_ar_conv_to_nova(Genode::uint32_t v)
{
	return (v & 0xff) | ((v & 0x1f000) >> 4);
}


static inline Genode::uint32_t sel_ar_conv_from_nova(Genode::uint16_t v)
{
	return (v & 0xff) | (((uint32_t )v << 4) & 0x1f000);
}


class Vcpu_handler : public Vmm::Vcpu_dispatcher<Genode::Thread>,
                     public Genode::List<Vcpu_handler>::Element
{
	protected:

		pthread::start_routine_t const _start_routine;
		void                   * const _start_routine_arg;

	private:

		X86FXSTATE _emt_fpu_state __attribute__((aligned(64)));

		pthread         _pthread;
		pthread_cond_t  _cond_wait;
		pthread_mutex_t _mutex;

		Vmm::Vcpu_other_pd _vcpu;

		Genode::addr_t     _ec_sel;
		bool               _irq_win = false;

		unsigned int       _cpu_id;
		unsigned int       _last_inj_info  = 0;
		unsigned int       _last_inj_error = 0;

		void fpu_save(char * data) {
			Assert(!(reinterpret_cast<Genode::addr_t>(data) & 0xF));
			asm volatile ("fxsave %0" : "=m" (*data));
		}

		void fpu_load(char * data) {
			Assert(!(reinterpret_cast<Genode::addr_t>(data) & 0xF));
			asm volatile ("fxrstor %0" : : "m" (*data));
		}

		enum {
			NOVA_REQ_IRQWIN_EXIT = 0x1000U,
			IRQ_INJ_VALID_MASK   = 0x80000000UL,
			IRQ_INJ_NONE         = 0U,

			/*
			 * Intel® 64 and IA-32 Architectures Software Developer’s Manual 
			 * Volume 3C, Chapter 24.4.2.
			 * May 2012
			*/
			BLOCKING_BY_STI    = 1U << 0,
			BLOCKING_BY_MOV_SS = 1U << 1,
			ACTIVITY_STATE_ACTIVE = 0U,
			INTERRUPT_STATE_NONE  = 0U,
		};

		timespec add_timespec_ns(timespec a, uint64_t ns) const
		{
			enum { NSEC_PER_SEC = 1'000'000'000ull };

			long sec = a.tv_sec;

			while (a.tv_nsec >= NSEC_PER_SEC) {
				a.tv_nsec -= NSEC_PER_SEC;
				sec++;
			}
			while (ns >= NSEC_PER_SEC) {
				ns -= NSEC_PER_SEC;
				sec++;
			}

			long nsec = a.tv_nsec + ns;
			while (nsec >= NSEC_PER_SEC) {
				nsec -= NSEC_PER_SEC;
				sec++;
			}
			return timespec { sec, nsec };
		}

	protected:

		__attribute__((noreturn)) void _longjmp()
		{
			longjmp(_env, 1);
		}

		/*
		 * 'longjmp()' restores some FPU registers saved by 'setjmp()',
		 * so we need to save the guest FPU state before calling 'longjmp()'
		 */
		__attribute__((noreturn)) void _fpu_save_and_longjmp()
		{
			_longjmp();
		}

		int map_memory(RTGCPHYS GCPhys,
		               size_t cbWrite, RTGCUINT vbox_fault_reason,
		               Genode::Flexpage_iterator &fli, bool &writeable);

		Genode::addr_t     _vm_exits    = 0;
		Genode::addr_t     _recall_skip = 0;
		Genode::addr_t     _recall_req  = 0;
		Genode::addr_t     _recall_inv  = 0;
		Genode::addr_t     _recall_drop = 0;
		Genode::addr_t     _irq_request = 0;
		Genode::addr_t     _irq_inject  = 0;
		Genode::addr_t     _irq_drop    = 0;

		struct {
			Nova::mword_t mtd;
			unsigned intr_state;
			unsigned ctrl[2];
		} next_utcb;

		PVM          _current_vm;
		PVMCPU       _current_vcpu;
		unsigned     _ept_fault_addr_type;
		void *       _stack_reply;
		jmp_buf      _env;

		Genode::uint64_t * pdpte_map(VM *pVM, RTGCPHYS cr3);

		void switch_to_hw()
		{
			unsigned long value;

			if (!setjmp(_env)) {
				_stack_reply = reinterpret_cast<void *>(
					Abi::stack_align(reinterpret_cast<Genode::addr_t>(&value)));
				Nova::reply(_stack_reply);
			}
		}

		__attribute__((noreturn)) void _default_handler()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);
			Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));

			_vm_exits ++;

			/* go back to VirtualBox */
			_fpu_save_and_longjmp();
		}

		__attribute__((noreturn)) void _recall_handler()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			_vm_exits ++;
			_recall_inv ++;

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			if (utcb->inj_info & IRQ_INJ_VALID_MASK) {

				Assert(utcb->flags & X86_EFL_IF);

				if (utcb->intr_state != INTERRUPT_STATE_NONE)
					Vmm::log("intr state ", Genode::Hex(utcb->intr_state),
					         " ", Genode::Hex(utcb->intr_state & 0xf));

				Assert(utcb->intr_state == INTERRUPT_STATE_NONE);

				if (!continue_hw_accelerated(utcb))
					_recall_drop ++;

				/* got recall during irq injection and the guest is ready for
				 * delivery of IRQ - just continue */
				Nova::reply(_stack_reply);
			}

			/* are we forced to go back to emulation mode ? */
			if (!continue_hw_accelerated(utcb)) {
				/* go back to emulation mode */
				_longjmp();
			}

			/* check whether we have to request irq injection window */
			if (check_to_request_irq_window(utcb, _current_vcpu)) {
				_irq_win = true;
				Nova::reply(_stack_reply);
			}

			unsigned long utcb_tpr    = utcb->read_tpr();
			bool interrupt_pending    = false;
			uint8_t tpr               = 0;
			uint8_t pending_interrupt = 0;
			PDMApicGetTPR(_current_vcpu, &tpr, &interrupt_pending, &pending_interrupt);

			/* we have a pending interrupt but nothing should be requested ? */
			if (interrupt_pending) {
				PDMApicSetTPR(_current_vcpu, utcb_tpr);

				_irq_win = check_to_request_irq_window(utcb, _current_vcpu);
				if (_irq_win) {
					Nova::reply(_stack_reply);
				}
			}

			/* nothing to do at all - continue hardware accelerated */

			/*
			 * Print a debug message if there actually IS something to do now.
			 * This can happen, for example, if one of the worker threads has
			 * set a flag in the meantime. Usually, setting a flag is followed
			 * by a recall request, but we haven't verified this for each flag
			 * yet.
			 */
			continue_hw_accelerated(utcb, true);

			if (_irq_win) {
				_irq_drop++;
				_irq_win = false;
				utcb->inj_info  = IRQ_INJ_NONE;
				utcb->mtd      |= Nova::Mtd::INJ;
			}

			Nova::reply(_stack_reply);
		}

		template <unsigned NPT_EPT>
		__attribute__((noreturn)) inline
		void _exc_memory(Genode::Thread * myself, Nova::Utcb * utcb,
		                 bool unmap, Genode::addr_t guest_fault,
		                 RTGCUINT vbox_errorcode)
		{
			using namespace Nova;
			using namespace Genode;

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			if (unmap) {
				Vmm::log("error: unmap not implemented");
				Nova::reply(_stack_reply);
			}

			enum { MAP_SIZE = 0x1000UL };

			bool writeable = true;
			Flexpage_iterator fli;

			Genode::addr_t gp_map_addr = guest_fault & ~((1UL << 12) - 1);
			int res = map_memory(gp_map_addr, MAP_SIZE, vbox_errorcode,
			                     fli, writeable);

			/* emulator has to take over if fault region is not ram */	
			if (res != VINF_SUCCESS) {
				/* see hmR0VmxExitEptViolation of VMM/VMMR0/HMVMXR0.cpp */ 
//				PVMCPU pVCpu = &_current_vm->aCpus[_cpu_id];
//				TRPMAssertXcptPF(pVCpu, guest_fault, vbox_errorcode);

				/* event re-injection is not handled yet for this case */
				Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));
				_longjmp();
			}

			/* fault region can be mapped - prepare utcb */
			utcb->set_msg_word(0);

			if (utcb->inj_info & IRQ_INJ_VALID_MASK) {
				/*
				 * The EPT violation occurred during event injection,
				 * so the event needs to be injected again.
				 */
				utcb->mtd |= Mtd::INJ;
				utcb->inj_info = _last_inj_info;
				utcb->inj_error = _last_inj_error;
			}

			enum {
				USER_PD   = false, GUEST_PGT = true,
				READABLE  = true, EXECUTABLE = true
			};

			Rights permission(READABLE, writeable, EXECUTABLE);


			/* add map items until no space is left on utcb anymore */
			do {
				Flexpage flexpage = fli.page();
				if (!flexpage.valid() || flexpage.log2_order < 12)
					break;

				/* touch memory - otherwise no mapping will take place */
				addr_t touch_me = flexpage.addr;
				while (touch_me < flexpage.addr + (1UL << flexpage.log2_order)) {
					touch_read(reinterpret_cast<unsigned char *>(touch_me));
					touch_me += 0x1000UL;
				}

				Crd crd  = Mem_crd(flexpage.addr >> 12, flexpage.log2_order - 12,
				                   permission);
				res = utcb->append_item(crd, flexpage.hotspot, USER_PD, GUEST_PGT);

				if (debug_map_memory)
					Vmm::log("map guest mem ", Genode::Hex(flexpage.addr),
					         "+", 1UL << flexpage.log2_order, " -> ", 
					         Genode::Hex(flexpage.hotspot), " ",
					         "guestf fault at ", Genode::Hex(guest_fault));
			} while (res);

			Nova::reply(_stack_reply);
		}

		/**
		 * Shortcut for calling 'Vmm::Vcpu_dispatcher::register_handler'
		 * with 'Vcpu_dispatcher' as template argument
		 */
		template <unsigned EV, void (Vcpu_handler::*FUNC)()>
		void _register_handler(Genode::addr_t exc_base, Nova::Mtd mtd)
		{
			if (!register_handler<EV, Vcpu_handler, FUNC>(exc_base, mtd))
				Genode::error("could not register handler ",
				              Genode::Hex(exc_base + EV));
		}


		Vmm::Vcpu_other_pd &vcpu() { return _vcpu; }


		inline bool vbox_to_utcb(Nova::Utcb * utcb, VM *pVM, PVMCPU pVCpu)
		{
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			::uint64_t tsc_aux = 0;
			auto const rc_tsc  = CPUMQueryGuestMsr(pVCpu, MSR_K8_TSC_AUX,
			                                       &tsc_aux);
			Assert(rc_tsc == VINF_SUCCESS);

			/* avoid utcb corruption by requesting tpr state early */
			bool interrupt_pending    = false;
			uint8_t tpr               = 0;
			uint8_t pending_interrupt = 0;
			PDMApicGetTPR(pVCpu, &tpr, &interrupt_pending, &pending_interrupt);

			/* don't call function hereafter which may corrupt the utcb ! */
			using namespace Nova;

			if (rc_tsc == VINF_SUCCESS) {
				utcb->mtd |= Mtd::TSC_AUX;
				utcb->tsc_aux = tsc_aux;
			}

			utcb->mtd |= Mtd::EIP;
			utcb->ip   = pCtx->rip;

			utcb->mtd |= Mtd::ESP;
			utcb->sp   = pCtx->rsp;

			utcb->mtd |= Mtd::ACDB;
			utcb->ax   = pCtx->rax;
			utcb->bx   = pCtx->rbx;
			utcb->cx   = pCtx->rcx;
			utcb->dx   = pCtx->rdx;

			utcb->mtd |= Mtd::EBSD;
			utcb->bp   = pCtx->rbp;
			utcb->si   = pCtx->rsi;
			utcb->di   = pCtx->rdi;

			utcb->mtd |= Mtd::R8_R15;
			utcb->write_r8(pCtx->r8);
			utcb->write_r9(pCtx->r9);
			utcb->write_r10(pCtx->r10);
			utcb->write_r11(pCtx->r11);
			utcb->write_r12(pCtx->r12);
			utcb->write_r13(pCtx->r13);
			utcb->write_r14(pCtx->r14);
			utcb->write_r15(pCtx->r15);

			utcb->mtd |= Mtd::EFL;
			utcb->flags = pCtx->rflags.u;

			utcb->mtd |= Mtd::SYS;
			utcb->sysenter_cs = pCtx->SysEnter.cs;
			utcb->sysenter_sp = pCtx->SysEnter.esp;
			utcb->sysenter_ip = pCtx->SysEnter.eip;

			utcb->mtd |= Mtd::DR;
			utcb->dr7  = pCtx->dr[7];

			utcb->mtd |= Mtd::CR;
			utcb->cr0  = pCtx->cr0;

			utcb->mtd |= Mtd::CR;
			utcb->cr2  = pCtx->cr2;

			utcb->mtd |= Mtd::CR;
			utcb->cr3  = pCtx->cr3;

			utcb->mtd |= Mtd::CR;
			utcb->cr4  = pCtx->cr4;

			utcb->mtd        |= Mtd::IDTR;
			utcb->idtr.limit  = pCtx->idtr.cbIdt;
			utcb->idtr.base   = pCtx->idtr.pIdt;

			utcb->mtd        |= Mtd::GDTR;
			utcb->gdtr.limit  = pCtx->gdtr.cbGdt;
			utcb->gdtr.base   = pCtx->gdtr.pGdt;

			utcb->mtd  |= Mtd::EFER;
			utcb->write_efer(CPUMGetGuestEFER(pVCpu));

			/*
			 * Update the PDPTE registers if necessary
			 *
			 * Intel manual sections 4.4.1 of Vol. 3A and 26.3.2.4 of Vol. 3C
			 * indicate the conditions when this is the case. The following
			 * code currently does not check if the recompiler modified any
			 * CR registers, which means the update can happen more often
			 * than really necessary.
			 */
			if (pVM->hm.s.vmx.fSupported &&
				CPUMIsGuestPagingEnabledEx(pCtx) &&
				CPUMIsGuestInPAEModeEx(pCtx)) {

				Genode::uint64_t *pdpte = pdpte_map(pVM, utcb->cr3);

				utcb->mtd |= Mtd::PDPTE;

				utcb->pdpte[0] = pdpte[0];
				utcb->pdpte[1] = pdpte[1];
				utcb->pdpte[2] = pdpte[2];
				utcb->pdpte[3] = pdpte[3];
			}

			utcb->mtd |= Mtd::SYSCALL_SWAPGS;
			utcb->write_star(pCtx->msrSTAR);
			utcb->write_lstar(pCtx->msrLSTAR);
			utcb->write_cstar(pCtx->msrCSTAR);
			utcb->write_fmask(pCtx->msrSFMASK);
			utcb->write_kernel_gs_base(pCtx->msrKERNELGSBASE);

			/* from HMVMXR0.cpp */
			utcb->mtd |= Mtd::TPR;
			utcb->write_tpr(tpr);
			utcb->write_tpr_threshold(0);
			if (interrupt_pending) {
				const uint8_t pending_priority = (pending_interrupt >> 4) & 0xf;
				const uint8_t tpr_priority = (tpr >> 4) & 0xf;
				if (pending_priority <= tpr_priority)
					utcb->write_tpr_threshold(pending_priority);
				else
					utcb->write_tpr_threshold(tpr_priority);
			}

			return true;
		}


		inline bool utcb_to_vbox(Nova::Utcb * utcb, VM *pVM, PVMCPU pVCpu)
		{
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			pCtx->rip = utcb->ip;
			pCtx->rsp = utcb->sp;

			pCtx->rax = utcb->ax;
			pCtx->rbx = utcb->bx;
			pCtx->rcx = utcb->cx;
			pCtx->rdx = utcb->dx;

			pCtx->rbp = utcb->bp;
			pCtx->rsi = utcb->si;
			pCtx->rdi = utcb->di;
			pCtx->rflags.u = utcb->flags;

			pCtx->r8  = utcb->read_r8();
			pCtx->r9  = utcb->read_r9();
			pCtx->r10 = utcb->read_r10();
			pCtx->r11 = utcb->read_r11();
			pCtx->r12 = utcb->read_r12();
			pCtx->r13 = utcb->read_r13();
			pCtx->r14 = utcb->read_r14();
			pCtx->r15 = utcb->read_r15();

			pCtx->dr[7] = utcb->dr7;

			if (pCtx->SysEnter.cs != utcb->sysenter_cs)
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_CS, utcb->sysenter_cs);

			if (pCtx->SysEnter.esp != utcb->sysenter_sp)
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_ESP, utcb->sysenter_sp);

			if (pCtx->SysEnter.eip != utcb->sysenter_ip)
				CPUMSetGuestMsr(pVCpu, MSR_IA32_SYSENTER_EIP, utcb->sysenter_ip);

			if (pCtx->idtr.cbIdt != utcb->idtr.limit ||
			    pCtx->idtr.pIdt  != utcb->idtr.base)
				CPUMSetGuestIDTR(pVCpu, utcb->idtr.base, utcb->idtr.limit);

			if (pCtx->gdtr.cbGdt != utcb->gdtr.limit ||
			    pCtx->gdtr.pGdt  != utcb->gdtr.base)
				CPUMSetGuestGDTR(pVCpu, utcb->gdtr.base, utcb->gdtr.limit);

			CPUMSetGuestEFER(pVCpu, utcb->read_efer());

			if (pCtx->cr0 != utcb->cr0)
				CPUMSetGuestCR0(pVCpu, utcb->cr0);

			if (pCtx->cr2 != utcb->cr2)
				CPUMSetGuestCR2(pVCpu, utcb->cr2);

			if (pCtx->cr3 != utcb->cr3) {
				CPUMSetGuestCR3(pVCpu, utcb->cr3);
				VMCPU_FF_SET(pVCpu, VMCPU_FF_HM_UPDATE_CR3);
			}

			if (pCtx->cr4 != utcb->cr4)
				CPUMSetGuestCR4(pVCpu, utcb->cr4);

			if (pCtx->msrSTAR != utcb->read_star())
				CPUMSetGuestMsr(pVCpu, MSR_K6_STAR, utcb->read_star());

			if (pCtx->msrLSTAR != utcb->read_lstar())
				CPUMSetGuestMsr(pVCpu, MSR_K8_LSTAR, utcb->read_lstar());

			if (pCtx->msrCSTAR != utcb->read_cstar())
				CPUMSetGuestMsr(pVCpu, MSR_K8_CSTAR, utcb->read_cstar());

			if (pCtx->msrSFMASK != utcb->read_fmask())
				CPUMSetGuestMsr(pVCpu, MSR_K8_SF_MASK, utcb->read_fmask());

			if (pCtx->msrKERNELGSBASE != utcb->read_kernel_gs_base())
				CPUMSetGuestMsr(pVCpu, MSR_K8_KERNEL_GS_BASE, utcb->read_kernel_gs_base());

			const uint32_t tpr = utcb->read_tpr();

			/* reset message transfer descriptor for next invocation */
			Assert (!(utcb->inj_info & IRQ_INJ_VALID_MASK));
			/* Reset irq window next time if we are still requesting it */
			next_utcb.mtd = _irq_win ? Nova::Mtd::INJ : 0;

			next_utcb.intr_state = utcb->intr_state;
			next_utcb.ctrl[0]    = utcb->ctrl[0];
			next_utcb.ctrl[1]    = utcb->ctrl[1];

			if (next_utcb.intr_state & 3) {
				next_utcb.intr_state &= ~3U;
				next_utcb.mtd        |= Nova::Mtd::STA;
			}

			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

			/* tell rem compiler that FPU register changed XXX optimizations ? */
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_FPU_REM); /* redundant ? XXX */
			pVCpu->cpum.s.fUseFlags |=  (CPUM_USED_FPU_GUEST | CPUM_USED_FPU_SINCE_REM); /* redundant ? XXX */
			
			if (utcb->intr_state != 0) {
				Assert(utcb->intr_state == BLOCKING_BY_STI ||
				       utcb->intr_state == BLOCKING_BY_MOV_SS);
				EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
			} else
				VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

			/* functions that corrupt utcb (e.g. when logging enabled) */
			PDMApicSetTPR(pVCpu, tpr);

			return true;
		}


		inline bool check_to_request_irq_window(Nova::Utcb * utcb, PVMCPU pVCpu)
		{
			if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS))
				return false;

			if (!TRPMHasTrap(pVCpu) &&
				!VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                             VMCPU_FF_INTERRUPT_PIC)))
				return false;

			_irq_request++;

			unsigned vector = 0;
			utcb->inj_info  = NOVA_REQ_IRQWIN_EXIT | vector;
			utcb->mtd      |= Nova::Mtd::INJ;

			return true;
		}


		__attribute__((noreturn)) void _irq_window()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			_vm_exits ++;

			PVMCPU   pVCpu = _current_vcpu;

			Assert(utcb->intr_state == INTERRUPT_STATE_NONE);
			Assert(utcb->flags & X86_EFL_IF);
			Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
			Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));

			Assert(_irq_win);
			_irq_win = false;

			/* request current tpr state from guest, it may block IRQs */
			PDMApicSetTPR(pVCpu, utcb->read_tpr());

			if (!TRPMHasTrap(pVCpu)) {

				bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
				if (res)
					Vmm::log("NMI was set");

				if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                               VMCPU_FF_INTERRUPT_PIC))) {

					uint8_t irq;
					int rc = PDMGetInterrupt(pVCpu, &irq);
					Assert(RT_SUCCESS(rc));

					rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
					Assert(RT_SUCCESS(rc));
				}

				if (!TRPMHasTrap(pVCpu)) {
					_irq_drop++;
					/* happens if PDMApicSetTPR (see above) mask IRQ */
					utcb->inj_info = IRQ_INJ_NONE;
					utcb->mtd      = Nova::Mtd::INJ;

					Nova::reply(_stack_reply);
				}
			}
			_irq_inject++;

			/*
			 * If we have no IRQ for injection, something with requesting the
			 * IRQ window went wrong. Probably it was forgotten to be reset.
			 */
			Assert(TRPMHasTrap(pVCpu));

		   	/* interrupt can be dispatched */
			uint8_t     u8Vector;
			TRPMEVENT   enmType;
			SVMEVENT    Event;
			RTGCUINT    u32ErrorCode;
			RTGCUINTPTR GCPtrFaultAddress;
			uint8_t     cbInstr;

			Event.u = 0;

			/* If a new event is pending, then dispatch it now. */
			int rc = TRPMQueryTrapAll(pVCpu, &u8Vector, &enmType, &u32ErrorCode, 0, 0);
			AssertRC(rc);
			Assert(enmType == TRPM_HARDWARE_INT);
			Assert(u8Vector != X86_XCPT_NMI);

			/* Clear the pending trap. */
			rc = TRPMResetTrap(pVCpu);
			AssertRC(rc);

			Event.n.u8Vector = u8Vector;
			Event.n.u1Valid  = 1;
			Event.n.u32ErrorCode = u32ErrorCode;

			Event.n.u3Type = SVM_EVENT_EXTERNAL_IRQ;

			utcb->inj_info  = Event.u; 
			utcb->inj_error = Event.n.u32ErrorCode;

			_last_inj_info = utcb->inj_info;
			_last_inj_error = utcb->inj_error;

/*
			Vmm::log("type:info:vector ", Genode::Hex(Event.n.u3Type),
			         Genode::Hex(utcb->inj_info), Genode::Hex(u8Vector),
			         " intr:actv - ", Genode::Hex(utcb->intr_state),
			         Genode::Hex(utcb->actv_state), " mtd ",
			         Genode::Hex(utcb->mtd));
*/
			utcb->mtd = Nova::Mtd::INJ;
			Nova::reply(_stack_reply);
		}


		inline bool continue_hw_accelerated(Nova::Utcb * utcb, bool verbose = false)
		{
			uint32_t check_vm = VM_FF_HM_TO_R3_MASK | VM_FF_REQUEST
			                    | VM_FF_PGM_POOL_FLUSH_PENDING
			                    | VM_FF_PDM_DMA;
			uint32_t check_vcpu = VMCPU_FF_HM_TO_R3_MASK
			                      | VMCPU_FF_PGM_SYNC_CR3
			                      | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
			                      | VMCPU_FF_REQUEST;

			if (!VM_FF_IS_PENDING(_current_vm, check_vm) &&
			    !VMCPU_FF_IS_PENDING(_current_vcpu, check_vcpu))
				return true;

			Assert(!(VM_FF_IS_PENDING(_current_vm, VM_FF_PGM_NO_MEMORY)));

#define VERBOSE_VM(flag) \
			do { \
				if (VM_FF_IS_PENDING(_current_vm, flag)) \
					Vmm::log("flag ", flag, " pending"); \
			} while (0)

#define VERBOSE_VMCPU(flag) \
			do { \
				if (VMCPU_FF_IS_PENDING(_current_vcpu, flag)) \
					Vmm::log("flag ", flag, " pending"); \
			} while (0)

			if (verbose) {
				/*
				 * VM_FF_HM_TO_R3_MASK
				 */
				VERBOSE_VM(VM_FF_TM_VIRTUAL_SYNC);
				VERBOSE_VM(VM_FF_PGM_NEED_HANDY_PAGES);
				/* handled by the assertion above */
				/* VERBOSE_VM(VM_FF_PGM_NO_MEMORY); */
				VERBOSE_VM(VM_FF_PDM_QUEUES);
				VERBOSE_VM(VM_FF_EMT_RENDEZVOUS);

				VERBOSE_VM(VM_FF_REQUEST);
				VERBOSE_VM(VM_FF_PGM_POOL_FLUSH_PENDING);
				VERBOSE_VM(VM_FF_PDM_DMA);

				/*
				 * VMCPU_FF_HM_TO_R3_MASK
				 */
				VERBOSE_VMCPU(VMCPU_FF_TO_R3);
				/* when this flag gets set, a recall request follows */
				/* VERBOSE_VMCPU(VMCPU_FF_TIMER); */
				VERBOSE_VMCPU(VMCPU_FF_PDM_CRITSECT);

				VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3);
				VERBOSE_VMCPU(VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
				VERBOSE_VMCPU(VMCPU_FF_REQUEST);
			}

#undef VERBOSE_VMCPU
#undef VERBOSE_VM

			return false;
		}

		virtual bool hw_load_state(Nova::Utcb *, VM *, PVMCPU) = 0;
		virtual bool hw_save_state(Nova::Utcb *, VM *, PVMCPU) = 0;
		virtual int vm_exit_requires_instruction_emulation(PCPUMCTX) = 0;

	public:

		enum Exit_condition
		{
			SVM_NPT     = 0xfc,
			SVM_INVALID = 0xfd,

			VCPU_STARTUP  = 0xfe,

			RECALL        = 0xff,
		};


		Vcpu_handler(Genode::Env &env, size_t stack_size,
		             pthread::start_routine_t start_routine, void *arg,
		             Genode::Cpu_connection * cpu_connection,
		             Genode::Affinity::Location location,
		             unsigned int cpu_id, const char * name,
		             Genode::Pd_session_capability pd_vcpu)
		:
			Vmm::Vcpu_dispatcher<Genode::Thread>(env, stack_size, cpu_connection,
			                                     location, name),
			_pthread(*this, stack_top()),
			_start_routine(start_routine),
			_start_routine_arg(arg),
			_vcpu(cpu_connection, location, pd_vcpu),
			_ec_sel(Genode::cap_map().insert()),
			_cpu_id(cpu_id)
		{
			pthread_mutexattr_t _attr;
			pthread_mutexattr_init(&_attr);

			pthread_cond_init(&_cond_wait, nullptr);

			pthread_mutexattr_settype(&_attr, PTHREAD_MUTEX_ERRORCHECK);
			pthread_mutex_init(&_mutex, &_attr);

		}

		pthread &pthread_obj() { return _pthread; }

		unsigned int cpu_id() { return _cpu_id; }

		Start_result start() override
		{
			_vcpu.start(_ec_sel);
			return Start_result::OK;
		}

		void recall(Vcpu_handler * other = nullptr)
		{
			using namespace Nova;

			asm volatile ("":::"memory");

			_recall_req ++;

			if (_irq_win) {
				_recall_skip ++;
				return;
			}

			if (ec_ctrl(EC_RECALL, _ec_sel) != NOVA_OK) {
				Genode::error("recall failed");
				Genode::sleep_forever();
			}

#if 0
			if (_recall_req % 1000 == 0) {
				using Genode::log;

				while (other) {
					log(other->_cpu_id, " exits=", other->_vm_exits,
					    " req:skip:drop,inv recall=", other->_recall_req, ":",
					    other->_recall_skip, ":", other->_recall_drop, ":",
					    other->_recall_inv, " req:inj:drop irq=",
					    other->_irq_request, ":", other->_irq_inject, ":",
					    other->_irq_drop);

					other = other->next();
				}
			}
#endif
		}

		void halt(Genode::uint64_t const wait_ns)
		{
			/* calculate timeout */
			timespec ts { 0, 0 };
			clock_gettime(CLOCK_REALTIME, &ts);
			ts = add_timespec_ns(ts, wait_ns);

			/* wait for condition or timeout */
			pthread_mutex_lock(&_mutex);
			pthread_cond_timedwait(&_cond_wait, &_mutex, &ts);
			pthread_mutex_unlock(&_mutex);
		}

		void wake_up()
		{
			pthread_mutex_lock(&_mutex);
			pthread_cond_signal(&_cond_wait);
			pthread_mutex_unlock(&_mutex);
		}

		int run_hw(PVMR0 pVMR0)
		{
			VM     * pVM   = reinterpret_cast<VM *>(pVMR0);
			PVMCPU   pVCpu = &pVM->aCpus[_cpu_id];
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			Assert(Thread::utcb() == Thread::myself()->utcb());

			Genode::addr_t old = pCtx->rip;

			/* take the utcb state prepared during the last exit */
			utcb->mtd        = next_utcb.mtd;
			utcb->inj_info   = IRQ_INJ_NONE;
			utcb->intr_state = next_utcb.intr_state;
			utcb->actv_state = ACTIVITY_STATE_ACTIVE;
			utcb->ctrl[0]    = next_utcb.ctrl[0];
			utcb->ctrl[1]    = next_utcb.ctrl[1];

			using namespace Nova;

			/* Transfer vCPU state from vBox to Nova format */
			if (!vbox_to_utcb(utcb, pVM, pVCpu) ||
				!hw_load_state(utcb, pVM, pVCpu)) {

				Genode::error("loading vCPU state failed");
				return VERR_INTERNAL_ERROR;
			}
			
			/* check whether to request interrupt window for injection */
			_irq_win = check_to_request_irq_window(utcb, pVCpu);

			/*
			 * Flag vCPU to be "pokeable" by external events such as interrupts
			 * from virtual devices. Only if this flag is set, the
			 * 'vmR3HaltGlobal1NotifyCpuFF' function calls 'SUPR3CallVMMR0Ex'
			 * with VMMR0_DO_GVMM_SCHED_POKE as argument to indicate such
			 * events. This function, in turn, will recall the vCPU.
			 */
			VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC);

			/* save current FPU state */
			fpu_save(reinterpret_cast<char *>(&_emt_fpu_state));
			/* write FPU state from pCtx to utcb */
			memcpy(utcb->fpu, pCtx->pXStateR3, sizeof(X86FXSTATE));
			/* tell kernel to transfer current fpu registers to vCPU */
			utcb->mtd |= Mtd::FPU;

			_current_vm   = pVM;
			_current_vcpu = pVCpu;

			/* switch to hardware accelerated mode */
			switch_to_hw();

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			_current_vm   = 0;
			_current_vcpu = 0;

			/* write FPU state of vCPU in utcb to pCtx */
			static_assert(sizeof(X86FXSTATE) <= sizeof(utcb->fpu), "fpu size mismatch");
			Genode::memcpy(pCtx->pXStateR3, utcb->fpu, sizeof(X86FXSTATE));

			/* load saved FPU state of EMT thread */
			fpu_load(reinterpret_cast<char *>(&_emt_fpu_state));

			/* see hmR0VmxExitToRing3 - sync recompiler state */
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_SYSENTER_MSR |
			                    CPUM_CHANGED_LDTR | CPUM_CHANGED_GDTR |
			                    CPUM_CHANGED_IDTR | CPUM_CHANGED_TR |
			                    CPUM_CHANGED_HIDDEN_SEL_REGS |
			                    CPUM_CHANGED_GLOBAL_TLB_FLUSH);

			VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

			/* Transfer vCPU state from Nova to vBox format */
			if (!utcb_to_vbox(utcb, pVM, pVCpu) ||
				!hw_save_state(utcb, pVM, pVCpu)) {

				Genode::error("saving vCPU state failed");
				return VERR_INTERNAL_ERROR;
			}

#ifdef VBOX_WITH_REM
			/* XXX see VMM/VMMR0/HMVMXR0.cpp - not necessary every time ! XXX */
			REMFlushTBs(pVM);
#endif

			/* track guest mode changes - see VMM/VMMAll/IEMAllCImpl.cpp.h */
			PGMChangeMode(pVCpu, pCtx->cr0, pCtx->cr4, pCtx->msrEFER);

			int rc = vm_exit_requires_instruction_emulation(pCtx);

			/* evaluated in VMM/include/EMHandleRCTmpl.h */
			return rc;
		}
};

#endif /* _VIRTUALBOX__SPEC__NOVA__VCPU_H_ */
