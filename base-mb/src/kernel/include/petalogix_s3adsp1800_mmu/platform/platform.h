/*
 * \brief  Implementations for kernels platform class
 * \author Martin Stein
 * \date   2010-10-01
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PETALOGIX_S3ADSP1800_MMU__PLATFORM__PLATFORM_H_
#define _INCLUDE__PETALOGIX_S3ADSP1800_MMU__PLATFORM__PLATFORM_H_

/* Device includes */
#include <xilinx/xps_intc.h>
#include <xilinx/xps_timer.h>
#include <xilinx/microblaze.h>

/* Kernel includes */
#include <kernel/types.h>
#include <generic/timer.h>
#include <generic/verbose.h>
#include <generic/blocking.h>


/*
 * Asm labels where to enter an irq/exception/syscall from userland, and vice
 * versa the userland from inside the kernel
 */
extern Kernel::word_t _syscall_entry;
extern Kernel::word_t _exception_entry;
extern Kernel::word_t _interrupt_entry;
extern Kernel::word_t _userland_entry;
extern Kernel::word_t _atomic_ops_begin;
extern Kernel::word_t _atomic_ops_end;
extern Kernel::addr_t _call_after_kernel;

namespace Kernel {

	enum { 
		PLATFORM__TRACE                   = 1,
		PLATFORM__VERBOSE                 = 0,
		PLATFORM__VERBOSE__THREAD_TRACING = 1,

		PLATFORM_THREAD__ERROR   = 1,
		PLATFORM_THREAD__WARNING = 1,
		PLATFORM_THREAD__VERBOSE = 0,

		PLATFORM_IRQ__VERBOSE       = 0,
		PLATFORM_EXCEPTION__VERBOSE = 0,
		PLATFORM_SYSCALL__VERBOSE   = 0,

		WORD_WIDTH_LOG2 = 5,
		BYTE_WIDTH_LOG2 = 3
	};

	class Platform_thread;
	class Platform;


	/**
	 * Get kernel's static platform representation
	 */
	Platform* platform();


	/**
	 * Platform specific execution context
	 */
	struct Exec_context
	{
		typedef Kernel::Protection_id Protection_id;
		typedef Kernel::word_t word_t;

		/**
		 * Type constraints
		 */
		enum{
			WORD_WIDTH_LOG2 = Kernel::WORD_WIDTH_LOG2,
			BYTE_WIDTH_LOG2 = Kernel::BYTE_WIDTH_LOG2,
			WORD_SIZE  = 1 << (WORD_WIDTH_LOG2-BYTE_WIDTH_LOG2) };

		/**
		 * Blocking types
		 */
		enum{
			NO_BLOCKING         = 0,
			IRQ_BLOCKING        = 1,
			EXCEPTION_BLOCKING  = 2,
			SYSCALL_BLOCKING    = 3,
			BLOCKING_TYPE_RANGE = 4
		};

		/**
		 * Register constraints
		 */
		enum {
			/* rmsr */
			RMSR_BE_LSHIFT  = 0,  RMSR_BE_MASK  = 1 << RMSR_BE_LSHIFT,
			RMSR_IE_LSHIFT  = 1,  RMSR_IE_MASK  = 1 << RMSR_IE_LSHIFT,
			RMSR_C_LSHIFT   = 2,  RMSR_C_MASK   = 1 << RMSR_C_LSHIFT,
			RMSR_BIP_LSHIFT = 3,  RMSR_BIP_MASK = 1 << RMSR_BIP_LSHIFT,
			RMSR_FSL_LSHIFT = 4,  RMSR_FSL_MASK = 1 << RMSR_FSL_LSHIFT,
			RMSR_ICE_LSHIFT = 5,  RMSR_ICE_MASK = 1 << RMSR_ICE_LSHIFT,
			RMSR_DZ_LSHIFT  = 6,  RMSR_DZ_MASK  = 1 << RMSR_DZ_LSHIFT,
			RMSR_DCE_LSHIFT = 7,  RMSR_DCE_MASK = 1 << RMSR_DCE_LSHIFT,
			RMSR_EE_LSHIFT  = 8,  RMSR_EE_MASK  = 1 << RMSR_EE_LSHIFT,
			RMSR_EIP_LSHIFT = 9,  RMSR_EIP_MASK = 1 << RMSR_EIP_LSHIFT,
			RMSR_PVR_LSHIFT = 10, RMSR_PVR_MASK = 1 << RMSR_PVR_LSHIFT,
			RMSR_UM_LSHIFT  = 11, RMSR_UM_MASK  = 1 << RMSR_UM_LSHIFT,
			RMSR_UMS_LSHIFT = 12, RMSR_UMS_MASK = 1 << RMSR_UMS_LSHIFT,
			RMSR_VM_LSHIFT  = 13, RMSR_VM_MASK  = 1 << RMSR_VM_LSHIFT,
			RMSR_VMS_LSHIFT = 14, RMSR_VMS_MASK = 1 << RMSR_VMS_LSHIFT,
			RMSR_CC_LSHIFT  = 31, RMSR_CC_MASK  = 1 << RMSR_CC_LSHIFT,

			/* resr */
			RESR_EC_LSHIFT  =  0, RESR_EC_MASK  = 0x1F<<RESR_EC_LSHIFT,
			RESR_ESS_LSHIFT =  5, RESR_ESS_MASK = 0x7F<<RESR_ESS_LSHIFT,
			RESR_DS_LSHIFT  = 12, RESR_DS_MASK  =    1<<RESR_DS_LSHIFT,

			/* resr-ess */
			RESR_ESS_DATA_TLB_MISS_S_LSHIFT = 5,
			RESR_ESS_DATA_TLB_MISS_S_MASK =
				1 << (RESR_ESS_LSHIFT + RESR_ESS_DATA_TLB_MISS_S_LSHIFT)
		};


		/**
		 * word_t offsets for execution context
		 */
		enum {
			/* General purpose registers */
			R0 = 0, R1 = 1, R2 = 2, R3 = 3, R4 = 4, R5 = 5, R6 = 6, R7 = 7, R8 = 8, R9 = 9,
			R10=10, R11=11, R12=12, R13=13, R14=14, R15=15, R16=16, R17=17, R18=18, R19=19,
			R20=20, R21=21, R22=22, R23=23, R24=24, R25=25, R26=26, R27=27, R28=28, R29=29,
			R30=30, R31=31,

			/* Special purpose registers */
			RPC = 32, RMSR = 33, REAR = 34, RESR = 35, RPID = 36,

			/* special state values */
			BLOCKING_TYPE = 37
		};

		enum{
			FIRST_GENERAL_PURPOSE_REGISTER =  0,
			LAST_GENERAL_PURPOSE_REGISTER  = 31,
			CONTEXT_WORD_SIZE              = 38
		};

		/**
		 * Execution context space read and written by the assembler
		 * kernel- and userland-entries
		 * Must be first member instance of Exec_context!
		 *
		 * Attention: Any changes in here have to be commited to
		 *            platforms exec_context_macros.s!
		 */
		word_t
			r0,  r1,  r2,  r3,  r4,  r5,  r6,  r7,  r8,  r9,
			r10, r11, r12, r13, r14, r15, r16, r17, r18, r19,
			r20, r21, r22, r23, r24, r25, r26, r27, r28, r29,
			r30, r31,
			rpc, rmsr, rear, resr, rpid, blocking_type;

		Platform_thread* holder;

		word_t word_at_offset(unsigned offset)
		{
			word_t* w=(word_t*)((uint32_t)this+offset*WORD_SIZE);
			return *w;
		}

		Exec_context(Platform_thread* h) : holder(h)
		{
			word_t* context = (word_t*)this;
			for (unsigned i = 0; i < CONTEXT_WORD_SIZE; i++)
				context[i] = 0;
		}

		void print_general_purpose_registers()
		{
			unsigned i = FIRST_GENERAL_PURPOSE_REGISTER;
			while(i <= LAST_GENERAL_PURPOSE_REGISTER) {

				if(!((i ^0) & 3)) {
					if(i) { printf("\n"); }
				}
				printf("r%2d=0x%8X  ", i, word_at_offset(i));
				i++;
			}
		}

		void print_special_purpose_registers()
		{
			printf("rpc=0x%8X rmsr=0x%8X rear=0x%8X resr=%8X rpid=%8X", 
			       rpc, rmsr, rear, resr, rpid);
		}

		void print_content(unsigned indent)
		{
			print_general_purpose_registers();
			printf("\n");
			print_special_purpose_registers(); 
			printf(" blocking_type=%i", blocking_type);
		}

		unsigned int exception_cause() {
			return (resr & RESR_EC_MASK) >> RESR_EC_LSHIFT; }
	};
}


extern Kernel::Exec_context* _userland_context;


namespace Kernel {

	/**
	 * Platform representation
	 */
	class Platform : public Kernel_entry::Listener
	{
		public:

			/**
			 * General configuration
			 */
			enum {
				ATOMIC_OPS_PAGE_SIZE_LOG2 = DEFAULT_PAGE_SIZE_LOG2,
				KERNEL_ENTRY_SIZE_LOG2    = DEFAULT_PAGE_SIZE_LOG2
			};

			/**
			 * Verbose, errors, warnings
			 */
			enum {
				VERBOSE__CONSTRUCTOR    = PLATFORM__VERBOSE,
				VERBOSE__ENTER_USERLAND = PLATFORM__VERBOSE
			};

			/**
			 * General platform constraints
			 */
			enum {
				WORD_WIDTH_LOG2 = Kernel::WORD_WIDTH_LOG2,
				BYTE_WIDTH_LOG2 = Kernel::BYTE_WIDTH_LOG2,
				BYTE_WIDTH = 1 <<  BYTE_WIDTH_LOG2,
				WORD_WIDTH = 1 <<  WORD_WIDTH_LOG2,
				WORD_SIZE  = 1 << (WORD_WIDTH_LOG2-BYTE_WIDTH_LOG2),

				WORD_HALFWIDTH = WORD_WIDTH >> 1,

				WORD_LEFTHALF_MASK  = ~0 << WORD_HALFWIDTH ,
				WORD_RIGHTHALF_MASK = ~WORD_LEFTHALF_MASK
			};

			typedef uint32_t word_t;
			typedef uint32_t Register;

		private:

			Kernel::Tlb _tlb;

			/**
			 * Processor specific
			 */
			enum {
				ASM_IMM  = 0xb0000000,
				ASM_BRAI = 0xb8080000,
				ASM_RTSD = 0xb6000000,
				ASM_NOP  = 0x80000000,

				SYSCALL_ENTRY   = 0x00000008,
				INTERRUPT_ENTRY = 0x00000010,
				EXCEPTION_ENTRY = 0x00000020
			};

			void _initial_tlb_entries()
			{
				using namespace Paging;

				Physical_page::size_t atomic_ops_pps, kernel_entry_pps;

				if (Physical_page::size_by_size_log2(
				     atomic_ops_pps, ATOMIC_OPS_PAGE_SIZE_LOG2) ||
				                     Physical_page::size_by_size_log2(
				                         kernel_entry_pps, KERNEL_ENTRY_SIZE_LOG2))
				{
					printf("Error in Kernel::Platform::_initial_tlb_entries");
					return;
				};

				Physical_page atomic_ops_pp((addr_t)&_atomic_ops_begin,
				                            atomic_ops_pps, Physical_page::RX);

				Virtual_page atomic_ops_vp(atomic_ops_pp.address(),
				                           UNIVERSAL_PROTECTION_ID);

				Resolution atomic_ops_res(&atomic_ops_vp, &atomic_ops_pp);

				Physical_page kernel_entry_pp((addr_t)0, kernel_entry_pps,
				                              Physical_page::RX);

				Virtual_page kernel_entry_vp(kernel_entry_pp.address(),
				                             UNIVERSAL_PROTECTION_ID);

				Resolution kernel_entry_res(&kernel_entry_vp, &kernel_entry_pp);

				tlb()->add_fixed(&atomic_ops_res, &kernel_entry_res);
			}

			/**
			 * Initialize the ability to enter userland
			 */
			inline void _init_userland_entry() {
				_call_after_kernel=(addr_t)&_userland_entry; }

			/**
			 * Fill in jump to CPU's 2-word-width exception entry
			 */
			inline void _init_exception_entry()
			{
				*(word_t*)EXCEPTION_ENTRY =
					ASM_IMM | ((word_t)&_exception_entry & WORD_LEFTHALF_MASK) >> WORD_HALFWIDTH;

				*(word_t*)(EXCEPTION_ENTRY + WORD_SIZE) =
					ASM_BRAI | ((word_t)&_exception_entry & WORD_RIGHTHALF_MASK);
			}

			/**
			 * Fill in jump to CPU's 2-word-width syscall entry
			 */
			inline void _init_syscall_entry()
			{
				*(word_t*)SYSCALL_ENTRY =
					ASM_IMM | ((word_t)&_syscall_entry & WORD_LEFTHALF_MASK) >> WORD_HALFWIDTH;

				*(word_t*)(SYSCALL_ENTRY + WORD_SIZE) =
					ASM_BRAI | (word_t) &_syscall_entry & WORD_RIGHTHALF_MASK; }

			/**
			 * Fill in jump to CPU's 2-word-width interrupt entry
			 */
			inline void _init_interrupt_entry()
			{
				*((word_t*) INTERRUPT_ENTRY) =
					ASM_IMM | ((word_t)&_interrupt_entry & WORD_LEFTHALF_MASK) >> WORD_HALFWIDTH;

				*((word_t*)(INTERRUPT_ENTRY + WORD_SIZE)) =
					ASM_BRAI | (word_t) &_interrupt_entry & WORD_RIGHTHALF_MASK;
			}

		public:

			/**
			 * Constructor
			 */
			Platform();

			bool is_atomic_operation(void* ip)
			{
				enum {
					SIZE       = (1 << ATOMIC_OPS_PAGE_SIZE_LOG2),
					SIZE_WORDS = SIZE/sizeof(word_t)
				};

				static word_t *const _first_atomic_op = &_atomic_ops_begin;
				static word_t *const _last_atomic_op =
					_first_atomic_op + (SIZE_WORDS-1);

				return ((ip >=_first_atomic_op) & (ip <=_last_atomic_op));
			}

			/**
			 * Set execution context loaded at next userland entry
			 */
			inline int userland_context(Exec_context* c)
			{
				_userland_context = c;
				_userland_context__verbose__set(c);
				return 0;
			}

			/**
			 * Lock the platforms execution ability to one execution context
			 */
			inline void lock(Exec_context *c){ userland_context(c); }

			/**
			 * Set return address register
			 *
			 * It is essential that this function is always inline!
			 */
			inline int return_address(addr_t a)
			{
				asm volatile("add r15, %0, r0"::"r"((Register)a):);
				return 0;
			}

			/**
			 * Halt whole system
			 */
			inline void halt() { asm volatile ("bri 0"); };

			/**
			 * Get the platforms general IRQ-controller
			 */
			inline Irq_controller * const irq_controller();

			/**
			 * Get the timer that is reserved for kernels schedulinge
			 */
			inline Scheduling_timer * const timer();

			Tlb *tlb() { return &_tlb; };

		protected:

			void _on_kernel_entry__trace__thread_interrupts();

			void _on_kernel_entry__verbose__called()
			{
				if (PLATFORM__VERBOSE)
					printf("Kernel::Platform::_on_kernel_entry\n");
			}

			void _on_kernel_entry();

			void _userland_context__verbose__set(Exec_context* c)
			{
				if (!PLATFORM__VERBOSE) return;
				if (_userland_context) {
					printf("Kernel::Platform::_userland_context, new userland context c=0x%8X, printing contents", (uint32_t)_userland_context);
					c->print_content(2);
				} else
					printf("Kernel::Platform::_userland_context, no userland context");

				printf("\n");
			}
	};


	class Platform_blocking
	{
		protected:

			typedef Kernel::Exec_context    Context;
			typedef Kernel::Platform_thread Owner;

			Owner*   _owner;
			Context* _context;

		public:

			/**
			 * Constructor
			 */
			Platform_blocking(Owner* o, Context* c)
			: _owner(o), _context(c) {}
	};


	/**
	 * Platform-specific IRQ
	 */
	class Platform_irq : public Platform_blocking, public Irq
	{
		public:

			/**
			 * Constructor
			 */
			Platform_irq(Owner* o, Context* c) : Platform_blocking(o,c) {}

			void block();

		protected:

			void _block__verbose__success()
			{
				if (PLATFORM_IRQ__VERBOSE)
					printf("Platform_irq::block(), _id=%i\n", _id);
			}
	};


	class Platform_exception : public Platform_blocking, public Exception
	{
		protected:

			Protection_id protection_id();
			addr_t          address();
			bool          attempted_write_access();

		public:

			/**
			 * Constructor
			 */
			Platform_exception(Owner* o, Context* c) : Platform_blocking(o, c) { }

			void block(Exec_context * c);
	};


	class Platform_syscall : public Platform_blocking, public Syscall
	{
		public:

			/**
			 * Constructor
			 */
			Platform_syscall(Owner *o, Context *c, Source *s) :
				Platform_blocking(o,c),
				Syscall(&c->r30, &c->r29, &c->r28,
				        &c->r27, &c->r26, &c->r25,
				        &c->r24, &c->r30, s)
			{ }

			void block();

		protected:

			void _block__verbose__success()
			{
				if (PLATFORM_IRQ__VERBOSE)
					printf("Platform_syscall::block(), _id=%i\n", _id);
			}
	};


	/**
	 * Platform-specific thread implementations
	 */
	class Platform_thread
	{
		typedef Kernel::Blocking Blocking;

		enum {
			INITIAL_RMSR = 1 << Exec_context::RMSR_PVR_LSHIFT
			             | 1 << Exec_context::RMSR_UMS_LSHIFT
			             | 1 << Exec_context::RMSR_VMS_LSHIFT,

			INITIAL_BLOCKING_TYPE = Exec_context::NO_BLOCKING
		};

		Platform_irq       _irq;
		Platform_exception _exception;
		Platform_syscall   _syscall;

		Exec_context _exec_context;

		/* if not zero, this thread is blocked */
		Blocking* _blocking;

		public:

			bool timer_interrupted()
			{
				return false;
			}

			void yield_after_atomic_operation() { _exec_context.r31 = 1; }

			void unblock() { _blocking = 0; }

			typedef Kernel::Protection_id Protection_id;

			Platform_thread() :
				_irq(this, &_exec_context),
				_exception(this, &_exec_context),
				_syscall(this, &_exec_context, 0),
				_exec_context(this),
				_blocking(0)
			{ }

			Platform_thread(addr_t             ip,
			                addr_t             sp,
			                Protection_id    pid,
			                Syscall::Source *sc)
			:
				_irq(this, &_exec_context),
				_exception(this, &_exec_context),
				_syscall(this, &_exec_context, sc),
				_exec_context(this),
				_blocking(0)
			{
				_exec_context.rpc          = ip;
				_exec_context.r1           = sp;
				_exec_context.rpid         = pid;
				_exec_context.blocking_type= INITIAL_BLOCKING_TYPE;
				_exec_context.rmsr         = INITIAL_RMSR; }

			enum { BLOCK__ERROR   = 1,
			       BLOCK__WARNING = 1};

			/**
			 * Get thread blocked if there is a blocking at execution context
			 */
			void on_kernel_entry()
			{
				using Kernel::Exec_context;

				switch (_exec_context.blocking_type) {

				case Exec_context::NO_BLOCKING:
					_blocking = 0;
					break;

				case Exec_context::IRQ_BLOCKING:
					_irq.block();
					_blocking = &_irq;
					break;

				case Exec_context::EXCEPTION_BLOCKING:
					_exception.block(&_exec_context); 
					_blocking = &_exception;
					break;

				case Exec_context::SYSCALL_BLOCKING:
					_syscall.block();
					_blocking = &_syscall;
					break;

				default:
					_block__error__unknown_blocking_type();
				}

				_block__verbose__success();
			}

			Protection_id protection_id() {
				return (Protection_id)_exec_context.rpid; }

			addr_t instruction_pointer() {
				return (addr_t)_exec_context.rpc; }

			Exec_context* exec_context() {
				return &_exec_context; }

			Exec_context* unblocked_exec_context()
			{
				Exec_context* result=&_exec_context;

				if (_blocking) {
					if (!_blocking->unblock())
						result = 0;
					else
						_blocking = 0;
				}
				return result;
			}

			void call_argument_0(word_t value){
				_exec_context.r5=value;}

			void bootstrap_argument_0(word_t value){
				_exec_context.r31=value;}

			void print_state() {
				_exec_context.print_content(2);
				printf("\n");
			};

			Exception *exception() { return &_exception; }
			Syscall   *syscall()   { return &_syscall; }
			Irq       *irq()       { return &_irq; }

		protected:

			void _block__error__unknown_blocking_type()
			{
				if (!PLATFORM_THREAD__ERROR)
					return;

				printf("Error in Kernel::Platform_thread::block: "
				       "unknown blocking_type=%i, printing state\n",
				       _exec_context.blocking_type);

				_exec_context.print_content(2);
				printf("halt\n");
				halt();
			}

			void _block__warning__no_blocking()
			{
				if (!PLATFORM_THREAD__WARNING)
					return;

				printf("Warning Kernel::Platform_thread::_no_blocking called\n");
				halt();
			}

			void _block__verbose__success()
			{
				if (!PLATFORM_THREAD__VERBOSE)
					return;

				printf("Kernel::Platform_thread::block, blocked "
				       "this=0x%p, blocking_type=%i\n",
				       this, _exec_context.blocking_type);
			}
	};
}


Kernel::Irq_controller * const Kernel::Platform::irq_controller()
{
	using namespace Xilinx;
	static Irq_controller _ic = Irq_controller(Xps_intc::Constr_arg(Cpu::XPS_INTC_BASE));
	return &_ic;
}


Kernel::Scheduling_timer * const Kernel::Platform::timer()
{
	using namespace Xilinx;
	static Scheduling_timer _st = Scheduling_timer(SCHEDULING_TIMER_IRQ, 
	                                               (addr_t)SCHEDULING_TIMER_BASE);
	return &_st;
}


#endif /* _INCLUDE__PETALOGIX_S3ADSP1800_MMU__PLATFORM__PLATFORM_H_ */

