/*
 * \brief  SUPLib vCPU utility
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Christian Helmuth
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__VCPU_H_
#define _VIRTUALBOX__VCPU_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <vm_session/connection.h>
#include <vm_session/handler.h>
#include <util/noncopyable.h>

/* local includes */
#include <sup.h>

namespace Sup {
	struct Vcpu_handler;
	struct Vcpu_handler_vmx;
	struct Vcpu_handler_svm;
}


class Sup::Vcpu_handler : Genode::Noncopyable
{
	protected:

		static Genode::Vm_connection::Exit_config const _exit_config;

		Genode::Entrypoint  _ep;
		Genode::Blockade    _blockade_emt { };
		Genode::Semaphore   _sem_handler;
		Genode::Vcpu_state *_state { nullptr };

		bool _last_exit_triggered_by_wrmsr = false;

		pthread_cond_t  _cond_wait;
		pthread_mutex_t _mutex;

		/* information used for NPT/EPT handling */
		Genode::addr_t _npt_ept_exit_addr { 0 };
		RTGCUINT       _npt_ept_errorcode { 0 };
		bool           _npt_ept_unmap     { false };

		/* state machine between EMT and EP thread of a vCPU */
		enum { RUNNING, PAUSED, IRQ_WIN, NPT_EPT } _vm_state { PAUSED };
		enum { PAUSE_EXIT, RUN } _next_state { RUN };

	private:

		bool           _irq_win = false;

		unsigned const _cpu_id;
		PVM            _vm   { nullptr };
		PVMCPU         _vcpu { nullptr };

		unsigned int   _last_inj_info  = 0;
		unsigned int   _last_inj_error = 0;

		enum {
			REQ_IRQWIN_EXIT      = 0x1000U,
			IRQ_INJ_VALID_MASK   = 0x80000000UL,
			IRQ_INJ_NONE         = 0U,

			/*
			 * Intel® 64 and IA-32 Architectures Software Developer’s Manual 
			 * Volume 3C, Chapter 24.4.2.
			 * May 2012
			*/
			ACTIVITY_STATE_ACTIVE              = 0U,
			INTERRUPT_STATE_NONE               = 0U,
			INTERRUPT_STATE_BLOCKING_BY_STI    = 1U << 0,
			INTERRUPT_STATE_BLOCKING_BY_MOV_SS = 1U << 1,
		};

		timespec _add_timespec_ns(timespec a, ::uint64_t ns) const;

		void _update_gim_system_time();

	protected:

		Genode::addr_t _vm_exits    = 0;
		Genode::addr_t _recall_skip = 0;
		Genode::addr_t _recall_req  = 0;
		Genode::addr_t _recall_inv  = 0;
		Genode::addr_t _recall_drop = 0;
		Genode::addr_t _irq_request = 0;
		Genode::addr_t _irq_inject  = 0;
		Genode::addr_t _irq_drop    = 0;

		struct {
			unsigned intr_state;
			unsigned ctrl[2];
		} _next_utcb;

		unsigned _ept_fault_addr_type;

		Genode::uint64_t * _pdpte_map(VM *pVM, RTGCPHYS cr3);

		void _switch_to_hw(PCPUMCTX pCtx);

		/* VM exit handlers */
		void _default_handler();
		bool _recall_handler();
		void _irq_window();
		void _npt_ept();

		void _irq_window_pthread();

		inline bool _vbox_to_state(VM *pVM, PVMCPU pVCpu);
		inline bool _state_to_vbox(VM *pVM, PVMCPU pVCpu);
		inline bool _check_to_request_irq_window(PVMCPU pVCpu);
		inline bool _continue_hw_accelerated();

		virtual bool _hw_load_state(VM *, PVMCPU) = 0;
		virtual bool _hw_save_state(VM *, PVMCPU) = 0;
		virtual int _vm_exit_requires_instruction_emulation(PCPUMCTX) = 0;

		virtual void _run_vm() = 0;
		virtual void _pause_vm() = 0;

	public:

		enum Exit_condition
		{
			SVM_NPT     = 0xfc,
			SVM_INVALID = 0xfd,

			VCPU_STARTUP  = 0xfe,

			RECALL        = 0xff,
		};

		Vcpu_handler(Genode::Env &env, size_t stack_size,
		             Genode::Affinity::Location location,
		             unsigned int cpu_id);

		unsigned int cpu_id() const { return _cpu_id; }

		void recall(VM &vm);

		void halt(Genode::uint64_t const wait_ns);

		void wake_up();

		int run_hw(VM &vm);
};


class Sup::Vcpu_handler_vmx : public Vcpu_handler
{
	private:

		Genode::Vcpu_handler<Vcpu_handler_vmx> _handler;

		Genode::Vm_connection       &_vm_connection;
		Genode::Vm_connection::Vcpu  _vcpu;

		/* VM exit handlers */
		void _vmx_default();
		void _vmx_startup();
		void _vmx_triple();
		void _vmx_irqwin();
		void _vmx_mov_crx();

		template <unsigned X> void _vmx_ept();
		__attribute__((noreturn)) void _vmx_invalid();

		void _handle_exit();

		void _run_vm()   override { _vcpu.run(); }
		void _pause_vm() override { _vcpu.pause(); }

		bool _hw_save_state(VM * pVM, PVMCPU pVCpu) override;
		bool _hw_load_state(VM * pVM, PVMCPU pVCpu) override;
		int  _vm_exit_requires_instruction_emulation(PCPUMCTX pCtx) override;

	public:

		Vcpu_handler_vmx(Genode::Env &env, size_t stack_size,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id,
		                 Genode::Vm_connection &vm_connection,
		                 Genode::Allocator &alloc);
};


class Sup::Vcpu_handler_svm : public Vcpu_handler
{
	private:

		Genode::Vcpu_handler<Vcpu_handler_svm> _handler;

		Genode::Vm_connection       &_vm_connection;
		Genode::Vm_connection::Vcpu  _vcpu;

		/* VM exit handlers */
		void _svm_default();
		void _svm_vintr();
		void _svm_ioio();

		template <unsigned X> void _svm_npt();

		void _svm_startup();

		void _handle_exit();

		void _run_vm()   override { _vcpu.run(); }
		void _pause_vm() override { _vcpu.pause(); }

		bool _hw_save_state(VM * pVM, PVMCPU pVCpu) override;
		bool _hw_load_state(VM * pVM, PVMCPU pVCpu) override;
		int  _vm_exit_requires_instruction_emulation(PCPUMCTX) override;

	public:

		Vcpu_handler_svm(Genode::Env &env, size_t stack_size,
		                 Genode::Affinity::Location location,
		                 unsigned int cpu_id,
		                 Genode::Vm_connection &vm_connection,
		                 Genode::Allocator &alloc);
};

#endif /* _VIRTUALBOX__VCPU_H_ */
