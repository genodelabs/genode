/*
 * \brief  Driver for the Xilinx LogiCORE XPS Timer/Counter IP 1.02
 * \author Martin stein
 * \date   2010-06-22
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DEVICES__XILINX_XPS_TIMER_H_
#define _INCLUDE__DEVICES__XILINX_XPS_TIMER_H_

#include <cpu/config.h>

namespace Xilinx {

	/**
	 * Driver for the Xilinx LogiCORE XPS Timer/Counter IP 1.02
	 */
	class Xps_timer
	{
		public:

			/**
			 * CPU dependencies
			 */
			typedef Cpu::word_t   word_t;
			typedef Cpu::addr_t   addr_t;
			typedef Cpu::size_t   size_t;
			typedef Cpu::uint32_t uint32_t;

			/**
			 * MMIO register
			 */
			typedef uint32_t Register;

			/**
			 * Constructor, resets timer, overwrites timer value with '0'
			 */
			inline Xps_timer(addr_t const &base);

			/**
			 * Destructor, resets timer, overwrites timer value with '0'
			 */
			inline ~Xps_timer();

			/**
			 * Overwrite timer value with X>0, count downwards to '0', set the
			 * IRQ output to '1' for one cycle and simultaneously start counting
			 * downwards again from X, and so on ...
			 *
			 * \param  value  the value X
			 */
			inline void run_periodic(unsigned int const &value);

			/**
			 * Overwrite timer value with X>0, count downwards to '0', set the
			 * IRQ output to '1' for one cycle and simultaneously start counting
			 * downwards from max_value() to '0', and so on ...
			 *
			 * \param  value  the value X
			 */
			inline void run_circulating(unsigned int const &value);

			/**
			 * Overwrite timer value with X>0, count downwards to '0', set the
			 * IRQ output to '1' for one cycle, timer value remains '0'
			 *
			 * \param  value  the value X
			 */
			inline void run_oneshot(unsigned int const &value);

			/**
			 * Prepare a 'run_oneshot()'-like run that shall be triggered with
			 * simple means. Useful for starting the timer out of assembly-code.
			 *
			 * \param  value  native time-value used to assess the delay
			 *                of the timer IRQ as of the triggering
			 * \param  start_val  at this address the start value gets deposited
			 * \param  start_reg  at this address an address X gets deposited
			 *                    writing the start value to X later starts the
			 *                    timer as prepared
			 */
			inline void prepare_oneshot(unsigned int const & value,
			                            volatile Register * & start_reg,
			                            Register & start_val);

			/**
			 * Current timer value
			 */
			inline unsigned int value();

			/**
			 * Return the timers current value and determine if the timer has hit '0'
			 * before the returned value and after the last time we started it or called 
			 * 'period_value' on it. Called during non-periodic runs 'rolled_over' 
			 * becomes 'true' if value is '0' and 'false' otherwise
			 * 
			 * Enable exclusive access only to this function to ensure correct behavior!
			 * This function delays the timer about the duration of a few cpu cycles!
			 */
			inline unsigned int period_value(bool * const & rolled_over);

			/**
			 * Size of the MMIO provided by the timer device
			 */
			static inline size_t size();

			/**
			 * Maximum timer value
			 */
			static inline unsigned int max_value();

			/**
			 * Converting a native time value to milliseconds
			 */
			static inline unsigned int native_to_msec(unsigned int const &v);

			/**
			 * Converting milliseconds to a native time value
			 */
			static inline unsigned int msec_to_native(unsigned int const &v);

			/**
			 * Converting a native time value to microseconds
			 */
			static inline unsigned int native_to_usec(unsigned int const &v);

			/**
			 * Converting microseconds to a native time value
			 */
			static inline unsigned int usec_to_native(unsigned int const &v);

		private:

			/**
			 * General constraints
			 */
			enum {
				WORD_SIZE        = sizeof(word_t),
				BYTE_WIDTH       = Cpu::BYTE_WIDTH,
				FREQUENCY_PER_US = 62,
			};

			/**
			 * Registers
			 */
			enum {
				/* Control/status register */
				RTCSR0_OFFSET = 0*WORD_SIZE,

				/* Load register, written to RTCR when RTCSR[LOAD]='1' */
				RTLR0_OFFSET  = 1*WORD_SIZE,

				/* On timer/counter register the counting is done */
				RTCR0_OFFSET  = 2*WORD_SIZE,

				/* Respectively for the second timer/counter module */
				RTCSR1_OFFSET = 4*WORD_SIZE,
				RTLR1_OFFSET  = 5*WORD_SIZE,
				RTCR1_OFFSET  = 6*WORD_SIZE,

				MMIO_SIZE = 8*WORD_SIZE,

				/* r/w '0': generate timer mode
				 * r/w '1': capture timer mode */
				RTCSR_MDT_LSHIFT  =  0,

				/* r/w '0': count upward mode
				 * r/w '1': count downward mode */
				RTCSR_UDT_LSHIFT  =  1,

				/* r/w '0': external generate signal disabled mode
				 * r/w '1': external generate signal enabled mode */
				RTCSR_GENT_LSHIFT =  2,

				/* r/w '0': external capture trigger disabled mode
				 * r/w '1': external capture trigger enabled mode */
				RTCSR_CAPT_LSHIFT =  3,

				/* r/w '0': hold values mode
				 * r/w '0': auto reload (generate timer) / overwrite (capture timer) mode */
				RTCSR_ARHT_LSHIFT =  4,

				/* w/r '0': disable loading mode
				 * w/r '1': loading timer mode (RTCR=RTLR) */
				RTCSR_LOAD_LSHIFT =  5,

				/* w/r '0': throw no IRQ mode (doesn't affect RTCSR[TINT])
				 * w/r '1': throw IRQ on 0-1-edge at RTCSR[TINT] mode */
				RTCSR_ENIT_LSHIFT =  6,

				/* w/r '0': don't count (RTCR remains constant)
				 * w/r '1': count on RTCR */
				RTCSR_ENT_LSHIFT  =  7,

				/* r '0': no IRQ has occured
				 * r '1': IRQ has occured
				 * w '0': no effect
				 * w '1': RTCSR[TINT]=0 */
				RTCSR_TINT_LSHIFT =  8,

				/* r/w '0': pulse width modulation disabled mode
				 * r/w '1': pulse width modulation enabled mode */
				RTCSR_PWM_LSHIFT =  9,

				/* w/r '0': nothing
				 * w/r '1': RTCSR[ENT]='1' for all timer/counter modules */
				RTCSR_ENALL_LSHIFT = 10,
			};

			/**
			 * Controls for RTCSR
			 */
			enum {
				RUN_ONCE = 0
				| 0 << RTCSR_MDT_LSHIFT
				| 1 << RTCSR_UDT_LSHIFT
				| 0 << RTCSR_CAPT_LSHIFT
				| 0 << RTCSR_GENT_LSHIFT
				| 0 << RTCSR_ARHT_LSHIFT
				| 0 << RTCSR_LOAD_LSHIFT
				| 1 << RTCSR_ENIT_LSHIFT
				| 1 << RTCSR_ENT_LSHIFT
				| 1 << RTCSR_TINT_LSHIFT
				| 0 << RTCSR_PWM_LSHIFT
				| 0 << RTCSR_ENALL_LSHIFT
				,

				STOP_N_LOAD = 0
				| 0 << RTCSR_MDT_LSHIFT
				| 1 << RTCSR_UDT_LSHIFT
				| 0 << RTCSR_CAPT_LSHIFT
				| 0 << RTCSR_GENT_LSHIFT
				| 0 << RTCSR_ARHT_LSHIFT
				| 1 << RTCSR_LOAD_LSHIFT
				| 0 << RTCSR_ENIT_LSHIFT
				| 0 << RTCSR_ENT_LSHIFT
				| 0 << RTCSR_TINT_LSHIFT
				| 0 << RTCSR_PWM_LSHIFT
				| 0 << RTCSR_ENALL_LSHIFT
				,

				RUN_PERIODIC = RUN_ONCE
				| 1 << RTCSR_ARHT_LSHIFT
				,

				STOP_N_RESET = STOP_N_LOAD
				| 1 << RTCSR_TINT_LSHIFT
				,
			};

			/**
			 * Absolute register addresses
			 */
			volatile Register *const _rtcsr0;
			volatile Register *const _rtlr0;
			volatile Register *const _rtcr0;
			volatile Register *const _rtcsr1;
			volatile Register *const _rtlr1;
			volatile Register *const _rtcr1;
	};
}


Xilinx::Xps_timer::Xps_timer(addr_t const &base)
:
	_rtcsr0((Register *)(base + RTCSR0_OFFSET)),
	_rtlr0((Register  *)(base + RTLR0_OFFSET)),
	_rtcr0((Register  *)(base + RTCR0_OFFSET)),
	_rtcsr1((Register *)(base + RTCSR1_OFFSET)),
	_rtlr1((Register  *)(base + RTLR1_OFFSET)),
	_rtcr1((Register  *)(base + RTCR1_OFFSET))
{
	*_rtcsr0 = STOP_N_RESET;
	*_rtcsr1 = STOP_N_RESET;
	*_rtlr0  = 0;
}


Xilinx::Xps_timer::size_t Xilinx::Xps_timer::size()
{
	return (size_t)MMIO_SIZE;
}


unsigned int Xilinx::Xps_timer::max_value() { return ((unsigned int)~0); }


void Xilinx::Xps_timer::prepare_oneshot(unsigned int const & value,
                                        volatile Register * & start_reg,
                                        Register & start_val)
{
	*_rtcsr0 = STOP_N_LOAD;
	*_rtlr0  = value;

	start_reg = _rtcsr0;
	start_val = RUN_ONCE;
}


void Xilinx::Xps_timer::run_circulating(unsigned int const &value)
{
	*_rtcsr0 = STOP_N_LOAD;
	*_rtlr0  = value;
	*_rtcsr0 = RUN_PERIODIC;
	*_rtlr0  = max_value();
}


void Xilinx::Xps_timer::run_periodic(unsigned int const &value)
{
	*_rtcsr0 = STOP_N_LOAD;
	*_rtlr0  = value;
	*_rtcsr0 = RUN_PERIODIC;
}


void Xilinx::Xps_timer::run_oneshot(unsigned int const &value)
{
	*_rtcsr0 = STOP_N_LOAD;
	*_rtlr0  = value;
	*_rtcsr0 = RUN_ONCE;
}


unsigned int Xilinx::Xps_timer::value() { return *_rtcr0; }


unsigned int Xilinx::Xps_timer::period_value(bool * const &rolled_over)
{
	if(!(*_rtcsr0 & (1 << RTCSR_ARHT_LSHIFT))){
		/* this is no periodic run */
		unsigned int const v = *_rtcr0;
		*rolled_over = !(v);
		return value(); 
	}

	/* 2 measurements are necessary to ensure that
	 * 'rolled_over' and the returned value are fit together 
	 * because we can not halt the timer or read both simulanously */
	unsigned int const v1 = *_rtcr0;
	*rolled_over = (bool)(*_rtcsr0 & (1 << RTCSR_TINT_LSHIFT));
	unsigned int const v2 = *_rtcr0;

	if(*rolled_over) {
		/* v2 must be a value the timer had after rolling over, so restart 
		 * the timer with the current value but RTCSR[TINT] reset */
		unsigned int const initial_rtlr = *_rtlr0;
		unsigned int const restart_n_reset = *_rtcsr0 | (1 << RTCSR_TINT_LSHIFT);
		*_rtlr0 = *_rtcr0;          // timer gets delayed about the
		*_rtcsr0 = restart_n_reset; // duration of these two operations
		*_rtlr0 = initial_rtlr;
		return v2; 
	}

	/* v1 must be a value that the timer had before rolling
	 * over, so we don't have to reset the "rolled over" status even 
	 * if the timer has rolled over till now */
	return v1;
}


Xilinx::Xps_timer::~Xps_timer()
{
	*_rtcsr0 = STOP_N_RESET;
	*_rtcsr1 = STOP_N_RESET;
}


unsigned int Xilinx::Xps_timer::native_to_msec(unsigned int const &v)
{
	return 1000*native_to_usec(v);
}


unsigned int Xilinx::Xps_timer::msec_to_native(unsigned int const &v)
{
	return 1000*usec_to_native(v);
}


unsigned int Xilinx::Xps_timer::native_to_usec(unsigned int const &v)
{
	return v/FREQUENCY_PER_US;
}


unsigned int Xilinx::Xps_timer::usec_to_native(unsigned int const &v)
{
	return v*FREQUENCY_PER_US;
}


#endif /* _INCLUDE__DEVICES__XILINX_XPS_TIMER_H_ */
