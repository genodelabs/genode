/*
 * \brief  Platform implementations
 * \author Martin stein
 * \date   2010-10-01
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Platform includes */
#include <cpu/prints.h>
#include <platform/platform.h>

/* Kernel includes */
#include <generic/printf.h>
#include <kernel/config.h>
#include <kernel/types.h>


extern unsigned int _current_context_label;
using namespace Kernel;
using namespace Cpu;


void Platform::_on_kernel_entry()
{
	_on_kernel_entry__verbose__called();
	_on_kernel_entry__trace__thread_interrupts();
	_userland_context->holder->on_kernel_entry();
	platform()->userland_context(0);
}


Platform::Platform()
{
	_initial_tlb_entries();
	_init_userland_entry();
	_init_interrupt_entry();
	_init_syscall_entry();
	_init_exception_entry();
}


void Platform_syscall::block()
{
	_id = (Syscall_id)_context->r31;
	_block__verbose__success();
}


void Platform_exception::block(Exec_context * c)
{
	_id = (Exception_id)((_context->resr & Context::RESR_EC_MASK) >> Context::RESR_EC_LSHIFT);
}


void Platform_irq::block()
{
	_id = platform()->irq_controller()->get_irq();
	
}


Protection_id Platform_exception::protection_id() {
	return _owner->protection_id(); }


addr_t Platform_exception::address(){ return (addr_t)_context->rear; }


bool Platform_exception::attempted_write_access(){
	return (_context->resr & Exec_context::RESR_ESS_DATA_TLB_MISS_S_MASK); }


static bool _trace_current_kernel_pass;

void Verbose::begin__trace_current_kernel_pass()
{
	enum {
		TRACED_PROTECTION_IDS =
			sizeof(trace_these_protection_ids)/
				sizeof(trace_these_protection_ids[0]),
		TRACED_THREAD_IDS=
			sizeof(trace_these_thread_ids)/
				sizeof(trace_these_thread_ids[0]),
		TRACED_EXCEPTION_IDS=
			sizeof(trace_these_exception_ids)/
				sizeof(trace_these_exception_ids[0]),
		TRACED_SYSCALL_IDS=
			sizeof(trace_these_syscall_ids)/
				sizeof(trace_these_syscall_ids[0])
	};


	if (!Verbose::TRACE_KERNEL_PASSES || !_userland_context) {
		_trace_current_kernel_pass = false;
		return;
	}

	if (!TRACE_ALL_THREAD_IDS) {
		for (unsigned int i = 0;; i++) {
			if (i >= TRACED_THREAD_IDS) {
				_trace_current_kernel_pass = false;
				return;
			}
			if (trace_these_thread_ids[i] == (Thread_id)_current_context_label)
				break;
		}
	}

	if (!TRACE_ALL_PROTECTION_IDS) {
		for (unsigned int i=0;; i++) {
			if (i>=TRACED_PROTECTION_IDS) {
				_trace_current_kernel_pass = false;
				return;
			}
			if (trace_these_protection_ids[i]
			 == (Protection_id)_userland_context->rpid) {
				_trace_current_kernel_pass = true;
				_prints_chr1('\n');
				return;
			}
		}
	}

	if (_userland_context->blocking_type == Exec_context::IRQ_BLOCKING
	 && !TRACE_ALL_IRQ_IDS) {

		_trace_current_kernel_pass = false;
		return;
	}

	if (_userland_context->blocking_type == Exec_context::EXCEPTION_BLOCKING
	 && !TRACE_ALL_EXCEPTION_IDS) {

		for (unsigned int i=0;; i++) {

			if (i >= TRACED_EXCEPTION_IDS) {
				_trace_current_kernel_pass = false;
				return;
			}
			if (trace_these_exception_ids[i] ==
			    (Exception_id)_userland_context->exception_cause()) {

				_trace_current_kernel_pass = true;
				_prints_chr1('\n');
				return;
			}
		}
	}

	if (_userland_context->blocking_type == Exec_context::EXCEPTION_BLOCKING
	 && !TRACE_ALL_EXCEPTION_IDS) {

		for (unsigned int i = 0;; i++) {
			if (i >= TRACED_EXCEPTION_IDS) {
				_trace_current_kernel_pass = false;
				return;
			}
			if (trace_these_exception_ids[i] == (Exception_id)_userland_context->r31) {
				_trace_current_kernel_pass=true;
				_prints_chr1('\n');
				return;
			}
		}
	}
	_trace_current_kernel_pass = false;
}


bool Kernel::Verbose::trace_current_kernel_pass()
{
	return _trace_current_kernel_pass;
}


void Platform::_on_kernel_entry__trace__thread_interrupts()
{
	if (!PLATFORM__TRACE) return;
	if (!Verbose::trace_current_kernel_pass()) return;

	_prints_str0("block(");
	_prints_hex2((char)_userland_context->rpid);
	_prints_str0(":");
	_prints_hex8(_userland_context->rpc);
	_prints_str0(":");
	_prints_hex2((char)_userland_context->blocking_type);

	char subtype=0;
	if (_userland_context->blocking_type == Exec_context::IRQ_BLOCKING){
		subtype=(char)platform()->irq_controller()->next_irq();
	} else if (_userland_context->blocking_type == Exec_context::EXCEPTION_BLOCKING){
		subtype=(char)_userland_context->resr;
	} else if (_userland_context->blocking_type == Exec_context::SYSCALL_BLOCKING){
		subtype=(char)_userland_context->r31;
	}

	_prints_str0(":");
	_prints_hex2(subtype);
	_prints_str0(") ");
}
