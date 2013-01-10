/*
 * \brief  Interface for irq controllers
 * \author Martin stein
 * \date   2010-06-21
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__IRQ_CONTROLLER_H_
#define _KERNEL__INCLUDE__GENERIC__IRQ_CONTROLLER_H_

#include <generic/blocking.h>
#include <generic/verbose.h>
#include <util/id_allocator.h>
#include <xilinx/xps_intc.h>


namespace Kernel {

	enum { 
		IRQ_CONTROLLER__VERBOSE = 0,
		BYTE_WIDTH=8,
	};


	template <typename DEVICE_T>
	class Irq_controller_tpl : public DEVICE_T
	{
		protected:

			/*************************
			 * Kernel_exit interface *
			 *************************/

			inline void _on_kernel_exit();

		public:

			/**
			 * Returns occured IRQ ID with highest priority and masks it
			 */
			inline Irq_id get_irq();

			/**
			 * Release IRQ and unmask it
			 */
			inline void ack_irq(Irq_id const & i);

			Irq_controller_tpl(typename DEVICE_T::Constr_arg const & dca);
	};

	typedef Irq_controller_tpl<Xilinx::Xps_intc> Irq_controller;

	class Irq_allocator :
		public Id_allocator<Thread, Irq_id, BYTE_WIDTH>
	{
		typedef Id_allocator<Thread, Irq_id, BYTE_WIDTH> Allocator;

		Irq_controller * const _controller;

		public:

			/**
			 * Error-codes that are returned by members
			 */
			enum Error {
				NO_ERROR = 0,
				HOLDER_DOESNT_OWN_IRQ = -1,
				IRQ_IS_PENDING_YET = -2,
				ALLOCATOR_ERROR = -3
			};

			/**
			 * Constructor
			 */
			Irq_allocator(Irq_controller * const ic) :
				_controller(ic)
			{ }

			/**
			 * Free IRQ if the TID-according thread owns it
			 */
			inline Error free(Thread * const t, Irq_id irq);

			/**
			 * Free IRQ if the TID-according thread owns it
			 */
			inline Error allocate(Thread * const t, Irq_id irq);
	};

	/**
	 * Pointer to kernels static IRQ allocator
	 */
	Irq_allocator * const irq_allocator();

	/**
	 * Pointer to kernels static IRQ controller
	 */
	Irq_controller * const irq_controller();
}


template <typename DEVICE_T>
Kernel::Irq_controller_tpl<DEVICE_T>::Irq_controller_tpl(typename DEVICE_T::Constr_arg const & dca) :
	DEVICE_T(dca)
{ }


template <typename DEVICE_T>
Kernel::Irq_id Kernel::Irq_controller_tpl<DEVICE_T>::get_irq()
{
	Irq_id const i = DEVICE_T::next_irq();
	DEVICE_T::mask(i);
	return i;
}


template <typename DEVICE_T>
void Kernel::Irq_controller_tpl<DEVICE_T>::ack_irq(Irq_id const & i)
{
	DEVICE_T::release(i);
	DEVICE_T::unmask(i);
}


Kernel::Irq_allocator::Error 
Kernel::Irq_allocator::allocate(Thread * const t, Irq_id irq)
{
	if (_controller->pending(irq)) { return IRQ_IS_PENDING_YET; }

	if (!Allocator::allocate(t, irq)) { return ALLOCATOR_ERROR; }

	_controller->unmask(irq);
	return NO_ERROR;
};


Kernel::Irq_allocator::Error 
Kernel::Irq_allocator::free(Thread * const t, Irq_id irq)
{
	if (_controller->pending(irq)) { return IRQ_IS_PENDING_YET; }

	Allocator::free(irq);
	_controller->mask(irq);
	return NO_ERROR;
};

#endif /* _KERNEL__INCLUDE__GENERIC__IRQ_CONTROLLER_H_ */
