/*
 * \brief  Declaration of gecoh timer device interface
 * \author Martin stein
 * \date   2010-06-23
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__TIMER_H_
#define _KERNEL__INCLUDE__GENERIC__TIMER_H_

#include <generic/verbose.h>
#include <generic/event.h>
#include <generic/blocking.h>
#include <generic/irq_controller.h>
#include <xilinx/xps_timer.h>
#include <cpu/config.h>

extern Cpu::uint32_t volatile * _kernel_timer_ctrl;
extern Cpu::uint32_t _kernel_timer_ctrl_start;

namespace Kernel {


	enum {
		TIMER__ERROR   = 1,
		TIMER__WARNING = 1,
		TIMER__VERBOSE = 0,
		TIMER__TRACE   = 1,
	};


	struct Tracks_time {
		virtual ~Tracks_time(){}
		virtual void time_consumed(unsigned int const & t) = 0;
	};


	template <typename DEVICE_T>
	class Timer : public Kernel_entry::Listener,
	              public Kernel_exit::Listener,
	              public DEVICE_T
	{
		private:

			Irq_id const  _irq_id;
			unsigned int  _start_value, _stop_value;
			Tracks_time * _client;

		protected:

			/* Kernel::Kernel_entry_event::Listener interface */
			void _on_kernel_entry();

			/* Kernel::Kernel_exit_event::Listener interface */
			void _on_kernel_exit();

			/* debugging */
			inline void _on_kernel_exit__error__start_value_invalid();
			inline void _on_kernel_entry__verbose__success();
			inline void _on_kernel_exit__verbose__success();

		public:

			Timer(Irq_id const & i, addr_t const & dca);

			inline bool is_busy();
			inline void track_time(unsigned int const & v, Tracks_time * const c);

			inline Irq_id irq_id();

			inline unsigned int stop_value();
			inline unsigned int start_value();

			/* debugging */
			void inline _start__trace(unsigned int const &v);
			void inline _stop__trace(unsigned int const &v);
	};


	typedef Timer<Xilinx::Xps_timer> Scheduling_timer;
}


/*******************
 ** Kernel::Timer **
 *******************/


template <typename DEVICE_T>
Kernel::Timer<DEVICE_T>::Timer(Irq_id const & i, 
                               addr_t const & dca) :
	DEVICE_T(dca),
	_irq_id(i),
	_start_value(0),
	_stop_value(0)
{ 
	irq_controller()->unmask(_irq_id);
}



template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_start__trace(unsigned int const &v)
{
	if (TIMER__TRACE && Verbose::trace_current_kernel_pass())
		printf("start(%i) ", v);
}


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_stop__trace(unsigned int const& v)
{
	if (TIMER__TRACE && Verbose::trace_current_kernel_pass())
		printf("stop(%i) ", v);
}


template <typename DEVICE_T>
unsigned int Kernel::Timer<DEVICE_T>::stop_value() { return _stop_value; }


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_on_kernel_exit__error__start_value_invalid()
{
	if (TIMER__ERROR)
		printf("Error in Kernel::Timer<DEVICE_T>::_on_kernel_exit,"
		       "_start_value=%i invalid\n", _start_value);
	halt();
}


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_on_kernel_entry__verbose__success()
{
	if (!TIMER__VERBOSE) return;

	printf("Kernel::Timer<DEVICE_T>::_on_kernel_entry,"
	       "_stop_value=%i\n", _stop_value);
}


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_on_kernel_exit__verbose__success()
{
	if (!TIMER__VERBOSE) return;

	printf("Kernel::Timer<DEVICE_T>::_on_kernel_exit,"
	       "_start_value=%i\n", _start_value);
}


template <typename DEVICE_T>
Kernel::Irq_id Kernel::Timer<DEVICE_T>::irq_id() { return _irq_id; }


template <typename DEVICE_T>
unsigned int Kernel::Timer<DEVICE_T>::start_value() { return _start_value; }


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::track_time(unsigned int const & v, Tracks_time * const c) { 
	_start_value=v; 
	_client = c; 
}


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_on_kernel_entry()
{
	_stop_value = DEVICE_T::value();
	_stop__trace(_stop_value);
	
	unsigned int t = start_value()- stop_value();
	_client->time_consumed(t);
	_on_kernel_entry__verbose__success();
}


template <typename DEVICE_T>
void Kernel::Timer<DEVICE_T>::_on_kernel_exit()
{
	if (!_start_value)
		_on_kernel_exit__error__start_value_invalid();

	_start__trace(_start_value);
	DEVICE_T::prepare_oneshot(_start_value, _kernel_timer_ctrl, _kernel_timer_ctrl_start);
}


#endif /* _KERNEL__INCLUDE__GENERIC__TIMER_H_ */

