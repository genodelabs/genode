/*
 * \brief  Genode/Nova specific VirtualBox SUPLib supplements
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__NOVA__VCPU_H_
#define _VIRTUALBOX__SPEC__NOVA__VCPU_H_

/* Genode includes */
#include <base/printf.h>
#include <base/semaphore.h>
#include <util/flex_iterator.h>
#include <rom_session/connection.h>
#include <timer_session/connection.h>

#include <vmm/vcpu_thread.h>
#include <vmm/vcpu_dispatcher.h>
#include <vmm/printf.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* VirtualBox includes */
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/vmm/pdmapi.h>

/* Genode's VirtualBox includes */
#include "sup.h"
#include "guest_memory.h"
#include "vmm_memory.h"

/* Genode libc pthread binding */
#include "thread.h"

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


/*
 * Used to map mmio memory to VM
 */
extern "C" int MMIO2_MAPPED_SYNC(PVM pVM, RTGCPHYS GCPhys, size_t cbWrite,
                                 void **ppv, Genode::Flexpage_iterator &fli,
                                 bool &writeable);


class Vcpu_handler : public Vmm::Vcpu_dispatcher<pthread>,
                     public Genode::List<Vcpu_handler>::Element
{
	private:

		X86FXSTATE _guest_fpu_state __attribute__((aligned(0x10)));
		X86FXSTATE _emt_fpu_state __attribute__((aligned(0x10)));

		Vmm::Vcpu_other_pd _vcpu;

		Genode::addr_t _ec_sel; 
		bool _irq_win;

		unsigned int      _cpu_id;
		Genode::Semaphore _halt_sem;

		unsigned int _last_inj_info;
		unsigned int _last_inj_error;

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

		/*
		 * 'longjmp()' restores some FPU registers saved by 'setjmp()',
		 * so we need to save the guest FPU state before calling 'longjmp()'
		 */
		__attribute__((noreturn)) void _fpu_save_and_longjmp()
		{
			fpu_save(reinterpret_cast<char *>(&_guest_fpu_state));
			longjmp(_env, 1);
		}

	protected:

		struct {
			Nova::mword_t mtd;
			unsigned intr_state;
			unsigned ctrl[2];
		} next_utcb;

		PVM      _current_vm;
		PVMCPU   _current_vcpu;
		void *   _stack_reply;
		jmp_buf  _env;

		void switch_to_hw()
		{
			unsigned long value;

			if (!setjmp(_env)) {
				_stack_reply = reinterpret_cast<void *>(&value - 1);
				Nova::reply(_stack_reply);
			}
		}

		__attribute__((noreturn)) void _default_handler()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);
			Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));

			/* go back to VirtualBox */
			_fpu_save_and_longjmp();
		}

		__attribute__((noreturn)) void _recall_handler()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			if (utcb->inj_info & IRQ_INJ_VALID_MASK) {

				Assert(utcb->flags & X86_EFL_IF);

				if (utcb->intr_state != INTERRUPT_STATE_NONE)
					Vmm::printf("intr state %x %x\n", utcb->intr_state, utcb->intr_state & 0xF);

				Assert(utcb->intr_state == INTERRUPT_STATE_NONE);

/*
				if (!continue_hw_accelerated(utcb))
					Vmm::printf("WARNING - recall ignored during IRQ delivery\n");
*/
				/* got recall during irq injection and the guest is ready for
				 * delivery of IRQ - just continue */
				Nova::reply(_stack_reply);
			}

			/* are we forced to go back to emulation mode ? */
			if (!continue_hw_accelerated(utcb)) {
				/* go back to emulation mode */
				_fpu_save_and_longjmp();
			}

			/* check whether we have to request irq injection window */
			utcb->mtd = Nova::Mtd::FPU;
			if (check_to_request_irq_window(utcb, _current_vcpu)) {
				_irq_win = true;
				Nova::reply(_stack_reply);
			}

			/* nothing to do at all - continue hardware accelerated */

			Assert(!_irq_win);

			/*
			 * Print a debug message if there actually IS something to do now.
			 * This can happen, for example, if one of the worker threads has
			 * set a flag in the meantime. Usually, setting a flag is followed
			 * by a recall request, but we haven't verified this for each flag
			 * yet.
			 */
			continue_hw_accelerated(utcb, true);

			Nova::reply(_stack_reply);
		}

		template <unsigned NPT_EPT>
		__attribute__((noreturn)) inline
		void _exc_memory(Genode::Thread * myself, Nova::Utcb * utcb,
		                 bool unmap, Genode::addr_t reason)
		{
			using namespace Nova;
			using namespace Genode;

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			if (unmap) {
				PERR("unmap not implemented\n");
				Nova::reply(_stack_reply);
			}

			enum { MAP_SIZE = 0x1000UL };

			bool writeable = true;
			Flexpage_iterator fli;
			void *pv = guest_memory()->lookup_ram(reason, MAP_SIZE, fli);

			if (!pv) {
				/**
				 * Check whether this is some mmio memory provided by VMM
				 * we can map, e.g. VMMDev memory or framebuffer currently.
				 */
				int res = MMIO2_MAPPED_SYNC(_current_vm, reason, MAP_SIZE, &pv,
				                            fli, writeable);
				if (res != VINF_SUCCESS)
					pv = 0;
			}

			/* emulator has to take over if fault region is not ram */	
			if (!pv) {
				/* event re-injection is not handled yet for this case */
				Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));
				_fpu_save_and_longjmp();
			}

			/* fault region can be mapped - prepare utcb */
			utcb->set_msg_word(0);
			utcb->mtd = Mtd::FPU;

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
			bool res;
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
					Vmm::printf("map guest mem %p+%x -> %lx - reason %lx\n",
					            flexpage.addr, 1UL << flexpage.log2_order,
					            flexpage.hotspot, reason);
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
				PERR("could not register handler %lx", exc_base + EV);
		}


		Vmm::Vcpu_other_pd &vcpu() { return _vcpu; }


		inline bool vbox_to_utcb(Nova::Utcb * utcb, VM *pVM, PVMCPU pVCpu)
		{
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			using namespace Nova;

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

				utcb->mtd |= Mtd::PDPTE;

				Genode::uint64_t *pdpte = (Genode::uint64_t*)
					guest_memory()->lookup(utcb->cr3, sizeof(utcb->pdpte));

				Assert(pdpte != 0);

				utcb->pdpte[0] = pdpte[0];
				utcb->pdpte[1] = pdpte[1];
				utcb->pdpte[2] = pdpte[2];
				utcb->pdpte[3] = pdpte[3];
			}

			utcb->mtd |= Mtd::SYSCALL_SWAPGS;
			utcb->write_star(pCtx->msrSTAR);
			utcb->write_lstar(pCtx->msrLSTAR);
			utcb->write_fmask(pCtx->msrSFMASK);
			utcb->write_kernel_gs_base(pCtx->msrKERNELGSBASE);

			/* from HMVMXR0.cpp */
			bool interrupt_pending    = false;
			uint8_t tpr               = 0;
			uint8_t pending_interrupt = 0;
			PDMApicGetTPR(pVCpu, &tpr, &interrupt_pending, &pending_interrupt);
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

			Assert(!(VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)));

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

			if (pCtx->cr3 != utcb->cr3)
				CPUMSetGuestCR3(pVCpu, utcb->cr3);

			if (pCtx->cr4 != utcb->cr4)
				CPUMSetGuestCR4(pVCpu, utcb->cr4);

			if (pCtx->msrSTAR != utcb->read_star())
				CPUMSetGuestMsr(pVCpu, MSR_K6_STAR, utcb->read_star());

			if (pCtx->msrLSTAR != utcb->read_lstar())
				CPUMSetGuestMsr(pVCpu, MSR_K8_LSTAR, utcb->read_lstar());

			if (pCtx->msrSFMASK != utcb->read_fmask())
				CPUMSetGuestMsr(pVCpu, MSR_K8_SF_MASK, utcb->read_fmask());

			if (pCtx->msrKERNELGSBASE != utcb->read_kernel_gs_base())
				CPUMSetGuestMsr(pVCpu, MSR_K8_KERNEL_GS_BASE, utcb->read_kernel_gs_base());

			PDMApicSetTPR(pVCpu, utcb->read_tpr());

			VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TO_R3);

			/* tell rem compiler that FPU register changed XXX optimizations ? */
			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_FPU_REM); /* redundant ? XXX */
			pVCpu->cpum.s.fUseFlags |=  (CPUM_USED_FPU | CPUM_USED_FPU_SINCE_REM); /* redundant ? XXX */
			
			if (utcb->intr_state != 0) {
				Assert(utcb->intr_state == BLOCKING_BY_STI ||
				       utcb->intr_state == BLOCKING_BY_MOV_SS);
				EMSetInhibitInterruptsPC(pVCpu, pCtx->rip);
			} else
				VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS);

			return true;
		}


		inline bool check_to_request_irq_window(Nova::Utcb * utcb, PVMCPU pVCpu)
		{
			if (!TRPMHasTrap(pVCpu) &&
				!VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                             VMCPU_FF_INTERRUPT_PIC)))
				return false;

			unsigned vector = 0;
			utcb->inj_info  = NOVA_REQ_IRQWIN_EXIT | vector;
			utcb->mtd      |= Nova::Mtd::INJ;

			return true;
		}


		__attribute__((noreturn)) void _irq_window()
		{
			Nova::Utcb * utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			PVMCPU   pVCpu = _current_vcpu;

			Assert(utcb->intr_state == INTERRUPT_STATE_NONE);
			Assert(utcb->flags & X86_EFL_IF);
			Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS));
			Assert(!(utcb->inj_info & IRQ_INJ_VALID_MASK));

			Assert(_irq_win);
			_irq_win = false;

			if (!TRPMHasTrap(pVCpu)) {

				bool res = VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_NMI);
				Assert(!res);

				if (VMCPU_FF_IS_PENDING(pVCpu, (VMCPU_FF_INTERRUPT_APIC |
				                               VMCPU_FF_INTERRUPT_PIC))) {

					uint8_t irq;
					int rc = PDMGetInterrupt(pVCpu, &irq);
					Assert(RT_SUCCESS(rc));

					rc = TRPMAssertTrap(pVCpu, irq, TRPM_HARDWARE_INT);
					Assert(RT_SUCCESS(rc));
				}
			}

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
			Vmm::printf("type:info:vector %x:%x:%x intr:actv - %x:%x mtd %x\n",
			     Event.n.u3Type, utcb->inj_info, u8Vector, utcb->intr_state, utcb->actv_state, utcb->mtd);
*/
			utcb->mtd = Nova::Mtd::INJ | Nova::Mtd::FPU;
			Nova::reply(_stack_reply);
		}


		inline bool continue_hw_accelerated(Nova::Utcb * utcb, bool verbose = false)
		{
			Assert(!(VMCPU_FF_IS_SET(_current_vcpu, VMCPU_FF_INHIBIT_INTERRUPTS)));

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
					Vmm::printf("flag " #flag " pending\n"); \
			} while (0)

#define VERBOSE_VMCPU(flag) \
			do { \
				if (VMCPU_FF_IS_PENDING(_current_vcpu, flag)) \
					Vmm::printf("flag " #flag " pending\n"); \
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
		virtual bool vm_exit_requires_instruction_emulation() = 0;

	public:

		enum Exit_condition
		{
			SVM_NPT     = 0xfc,
			SVM_INVALID = 0xfd,

			VCPU_STARTUP  = 0xfe,

			RECALL        = 0xff,
			EMULATE_INSTR = 0x100
		};


		Vcpu_handler(size_t stack_size, const pthread_attr_t *attr,
		             void *(*start_routine) (void *), void *arg,
		             Genode::Cpu_session * cpu_session,
		             Genode::Affinity::Location location,
		             unsigned int cpu_id)
		:
			Vmm::Vcpu_dispatcher<pthread>(stack_size, *Genode::env()->pd_session(),
			                              cpu_session, location, 
			                              attr ? *attr : 0, start_routine, arg),
			_vcpu(cpu_session, location),
			_ec_sel(Genode::cap_map()->insert()),
			_irq_win(false),
			_cpu_id(cpu_id)
		{ }

		unsigned int cpu_id() { return _cpu_id; }

		void start() {
			_vcpu.start(_ec_sel);
		}

		void recall()
		{
			using namespace Nova;

			if (ec_ctrl(EC_RECALL, _ec_sel) != NOVA_OK) {
				PERR("recall failed");
				Genode::Lock lock(Genode::Lock::LOCKED);
				lock.lock();
			}
		}

		void halt()
		{
			_halt_sem.down();
		}

		void wake_up()
		{
			_halt_sem.up();
		}

		inline void dump_register_state(PCPUMCTX pCtx)
		{
			PINF("pCtx");
			PLOG("ip:sp:efl ax:bx:cx:dx:si:di %llx:%llx:%llx"
			     " %llx:%llx:%llx:%llx:%llx:%llx",
			     pCtx->rip, pCtx->rsp, pCtx->rflags.u, pCtx->rax, pCtx->rbx,
			     pCtx->rcx, pCtx->rdx, pCtx->rsi, pCtx->rdi);

			PLOG("cs.attr.n.u4LimitHigh=0x%x", pCtx->cs.Attr.n.u4LimitHigh);

			PLOG("cs base:limit:sel:ar %llx:%x:%x:%x", pCtx->cs.u64Base,
			     pCtx->cs.u32Limit, pCtx->cs.Sel, pCtx->cs.Attr.u);
			PLOG("ds base:limit:sel:ar %llx:%x:%x:%x", pCtx->ds.u64Base,
			     pCtx->ds.u32Limit, pCtx->ds.Sel, pCtx->ds.Attr.u);
			PLOG("es base:limit:sel:ar %llx:%x:%x:%x", pCtx->es.u64Base,
			     pCtx->es.u32Limit, pCtx->es.Sel, pCtx->es.Attr.u);
			PLOG("fs base:limit:sel:ar %llx:%x:%x:%x", pCtx->fs.u64Base,
			     pCtx->fs.u32Limit, pCtx->fs.Sel, pCtx->fs.Attr.u);
			PLOG("gs base:limit:sel:ar %llx:%x:%x:%x", pCtx->gs.u64Base,
			     pCtx->gs.u32Limit, pCtx->gs.Sel, pCtx->gs.Attr.u);
			PLOG("ss base:limit:sel:ar %llx:%x:%x:%x", pCtx->ss.u64Base,
			     pCtx->ss.u32Limit, pCtx->ss.Sel, pCtx->ss.Attr.u);

			PLOG("cr0:cr2:cr3:cr4 %llx:%llx:%llx:%llx",
			     pCtx->cr0, pCtx->cr2, pCtx->cr3, pCtx->cr4);

			PLOG("ldtr base:limit:sel:ar %llx:%x:%x:%x", pCtx->ldtr.u64Base,
			     pCtx->ldtr.u32Limit, pCtx->ldtr.Sel, pCtx->ldtr.Attr.u);
			PLOG("tr base:limit:sel:ar %llx:%x:%x:%x", pCtx->tr.u64Base,
			     pCtx->tr.u32Limit, pCtx->tr.Sel, pCtx->tr.Attr.u);

			PLOG("gdtr base:limit %llx:%x", pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt);
			PLOG("idtr base:limit %llx:%x", pCtx->idtr.pIdt, pCtx->idtr.cbIdt);

			PLOG("dr 0:1:2:3:4:5:6:7 %llx:%llx:%llx:%llx:%llx:%llx:%llx:%llx",
			     pCtx->dr[0], pCtx->dr[1], pCtx->dr[2], pCtx->dr[3],
			     pCtx->dr[4], pCtx->dr[5], pCtx->dr[6], pCtx->dr[7]);

			PLOG("sysenter cs:eip:esp %llx %llx %llx", pCtx->SysEnter.cs,
			     pCtx->SysEnter.eip, pCtx->SysEnter.esp);
		}

		inline void dump_register_state(Nova::Utcb * utcb)
		{
			PINF("utcb");
			PLOG("ip:sp:efl ax:bx:cx:dx:si:di %lx:%lx:%lx"
			     " %lx:%lx:%lx:%lx:%lx:%lx",
			     utcb->ip, utcb->sp, utcb->flags, utcb->ax, utcb->bx,
			     utcb->cx, utcb->dx, utcb->si, utcb->di);

			PLOG("cs base:limit:sel:ar %lx:%x:%x:%x", utcb->cs.base,
			     utcb->cs.limit, utcb->cs.sel, utcb->cs.ar);
			PLOG("ds base:limit:sel:ar %lx:%x:%x:%x", utcb->ds.base,
			     utcb->ds.limit, utcb->ds.sel, utcb->ds.ar);
			PLOG("es base:limit:sel:ar %lx:%x:%x:%x", utcb->es.base,
			     utcb->es.limit, utcb->es.sel, utcb->es.ar);
			PLOG("fs base:limit:sel:ar %lx:%x:%x:%x", utcb->fs.base,
			     utcb->fs.limit, utcb->fs.sel, utcb->fs.ar);
			PLOG("gs base:limit:sel:ar %lx:%x:%x:%x", utcb->gs.base,
			     utcb->gs.limit, utcb->gs.sel, utcb->gs.ar);
			PLOG("ss base:limit:sel:ar %lx:%x:%x:%x", utcb->ss.base,
			     utcb->ss.limit, utcb->ss.sel, utcb->ss.ar);

			PLOG("cr0:cr2:cr3:cr4 %lx:%lx:%lx:%lx",
			     utcb->cr0, utcb->cr2, utcb->cr3, utcb->cr4);

			PLOG("ldtr base:limit:sel:ar %lx:%x:%x:%x", utcb->ldtr.base,
			     utcb->ldtr.limit, utcb->ldtr.sel, utcb->ldtr.ar);
			PLOG("tr base:limit:sel:ar %lx:%x:%x:%x", utcb->tr.base,
			     utcb->tr.limit, utcb->tr.sel, utcb->tr.ar);

			PLOG("gdtr base:limit %lx:%x", utcb->gdtr.base, utcb->gdtr.limit);
			PLOG("idtr base:limit %lx:%x", utcb->idtr.base, utcb->idtr.limit);

			PLOG("dr 7 %lx", utcb->dr7);

			PLOG("sysenter cs:eip:esp %lx %lx %lx", utcb->sysenter_cs,
			     utcb->sysenter_ip, utcb->sysenter_sp);

			PLOG("%x %x %x", utcb->intr_state, utcb->actv_state, utcb->mtd);
		}

		int run_hw(PVMR0 pVMR0)
		{
			VM     * pVM   = reinterpret_cast<VM *>(pVMR0);
			PVMCPU   pVCpu = &pVM->aCpus[_cpu_id];
			PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(Thread::utcb());

			Assert(Thread::utcb() == Thread::myself()->utcb());

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

				PERR("loading vCPU state failed");
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
			/* write FPU state from pCtx to FPU registers */
			fpu_load(reinterpret_cast<char *>(&pCtx->fpu));
			/* tell kernel to transfer current fpu registers to vCPU */
			utcb->mtd |= Mtd::FPU;

			_current_vm   = pVM;
			_current_vcpu = pVCpu;

			/* switch to hardware accelerated mode */
			switch_to_hw();

			Assert(utcb->actv_state == ACTIVITY_STATE_ACTIVE);

			_current_vm   = 0;
			_current_vcpu = 0;

			/* write FPU state of vCPU (in current FPU registers) to pCtx */
			Genode::memcpy(&pCtx->fpu, &_guest_fpu_state, sizeof(X86FXSTATE));

			/* load saved FPU state of EMT thread */
			fpu_load(reinterpret_cast<char *>(&_emt_fpu_state));

			CPUMSetChangedFlags(pVCpu, CPUM_CHANGED_GLOBAL_TLB_FLUSH);

			VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);

			/* Transfer vCPU state from Nova to vBox format */
			if (!utcb_to_vbox(utcb, pVM, pVCpu) ||
				!hw_save_state(utcb, pVM, pVCpu)) {

				PERR("saving vCPU state failed");
				return VERR_INTERNAL_ERROR;
			}

			/* reset message transfer descriptor for next invocation */
			Assert (!(utcb->inj_info & IRQ_INJ_VALID_MASK));
			/* Reset irq window next time if we are still requesting it */
			next_utcb.mtd = _irq_win ? Mtd::INJ : 0;

			next_utcb.intr_state = utcb->intr_state;
			next_utcb.ctrl[0]    = utcb->ctrl[0];
			next_utcb.ctrl[1]    = utcb->ctrl[1];

			if (next_utcb.intr_state & 3) {
				next_utcb.intr_state &= ~3U;
				next_utcb.mtd        |= Mtd::STA;
			}

#ifdef VBOX_WITH_REM
			/* XXX see VMM/VMMR0/HMVMXR0.cpp - not necessary every time ! XXX */
			REMFlushTBs(pVM);
#endif

			return vm_exit_requires_instruction_emulation() ? VINF_EM_RAW_EMULATE_INSTR
			                                                : VINF_SUCCESS;
		}
};

#endif /* _VIRTUALBOX__SPEC__NOVA__VCPU_H_ */
