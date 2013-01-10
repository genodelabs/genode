/*
 * \brief  Macros for errors, warnings, debugging
 * \author Martin stein
 * \date   2010-06-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__VERBOSE_H_
#define _KERNEL__INCLUDE__VERBOSE_H_

/* kernel includes */
#include <kernel/types.h>

/* OS includes */
#include <base/printf.h>
#include <cpu/prints.h>

using Genode::printf;

namespace Kernel {

	using namespace Cpu;

	/**
	 * Halt all executions (uninteruptable endless loop)
	 */
	void halt();

	unsigned word_width();

	/**
	 * Implementing verbose helper methods
	 */
	namespace Verbose {

		enum {
			TRACE_KERNEL_PASSES      = 0,
			TRACE_ALL_THREAD_IDS     = 1,
			TRACE_ALL_PROTECTION_IDS = 1,
			TRACE_ALL_SYSCALL_IDS    = 1,
			TRACE_ALL_EXCEPTION_IDS  = 1,
			TRACE_ALL_IRQ_IDS        = 1
		};

		Kernel::Thread_id const trace_these_thread_ids[]= { 0 };

		Kernel::Protection_id const trace_these_protection_ids[] = {
			Roottask::PROTECTION_ID, Kernel::INVALID_PROTECTION_ID };

		Kernel::Syscall_id const trace_these_syscall_ids[] = { INVALID_SYSCALL_ID };
//			TLB_LOAD     ,
//			TLB_FLUSH    ,
//			THREAD_CREATE,
//			THREAD_KILL  ,
//			THREAD_SLEEP ,
//			THREAD_WAKE  ,
//			THREAD_YIELD ,
//			THREAD_PAGER ,
//			IPC_REQUEST  ,
//			IPC_SERVE    ,
//			PRINT_CHAR   ,
//			PRINT_INFO   ,

		Kernel::Exception_id const trace_these_exception_ids[] = { INVALID_EXCEPTION_ID };
//			FAST_SIMPLEX_LINK     ,
//			UNALIGNED             ,
//			ILLEGAL_OPCODE        ,
//			INSTRUCTION_BUS       ,
//			DATA_BUS              ,
//			DIV_BY_ZERO_EXCEPTON  ,
//			FPU                   ,
//			PRIVILEGED_INSTRUCTION,
//			INTERRUPT                  ,
//			EXTERNAL_NON_MASKABLE_BREAK,
//			EXTERNAL_MASKABLE_BREAK    ,
//			DATA_STORAGE        ,
//			INSTRUCTION_STORAGE ,
//			DATA_TLB_MISS       ,
//			INSTRUCTION_TLB_MISS,

		/*
		 * Tracing for specific kernel-entry causes can be configured in
		 * 'platform.cc'.
		 */
		bool trace_current_kernel_pass();

		void begin__trace_current_kernel_pass();

		void inline indent(unsigned int const &i);
	}
}


void Kernel::Verbose::indent(unsigned int const &indent)
{
	for (unsigned int i = 0; i < indent; i++)
		_prints_chr1(' ');
}


#endif /* _KERNEL__INCLUDE__VERBOSE_H_ */
