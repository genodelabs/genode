/*
 * \brief Qemu USB controller interface
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date 2015-12-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__QEMU__USB_H_
#define _INCLUDE__QEMU__USB_H_

#include <base/stdint.h>
#include <base/signal.h>

namespace Qemu {

	typedef Genode::size_t  size_t;
	typedef Genode::off_t   off_t;
	typedef Genode::int64_t int64_t;
	typedef Genode::addr_t  addr_t;


	/************************************
	 ** Qemu library backend interface **
	 ************************************/

	/*
	 * The backend interface needs to be provided the user of the library.
	 */

	/**
	 * Timer_queue class
	 *
	 * The library uses the Timer_queue internally to schedule timeouts.
	 */
	struct Timer_queue
	{
		virtual int64_t get_ns() = 0;
		virtual void    register_timer(void *qtimer, void (*cb)(void *data), void *data) = 0;
		virtual void    delete_timer(void *qtimer) = 0;
		virtual void    activate_timer(void *qtimer, long long int expires_abs) = 0;
		virtual void    deactivate_timer(void *qtimer) = 0;
	};

	/**
	 * Pci_device class
	 *
	 * The library uses the Pci_device to access physical DMA memory and to
	 * raise interrupts.
	 */
	struct Pci_device
	{
		/**
		 * Raise interrupt
		 *
		 * \param assert 1 == raise, 0 == deassert interrupt
		 */
		virtual void  raise_interrupt(int assert) = 0;
		virtual int   read_dma(addr_t addr, void *buf, size_t size) = 0;
		virtual int   write_dma(addr_t addr, void const *buf, size_t size) = 0;
		virtual void *map_dma(addr_t base, size_t size) = 0;
		virtual void  unmap_dma(void *addr, size_t size) = 0;
	};


	/*************************************
	 ** Qemu library frontend functions **
	 *************************************/

	/**
	 * Controller class to access MMIO register of the xHCI device model
	 *
	 * This class is implemented by the library.
	 */
	struct Controller
	{
		/**
		 * Size of the MMIO region of the controller
		 */
		virtual size_t mmio_size() const = 0;

		/**
		 * Read/write MMIO offset in the MMIO region of the controller
		 */
		virtual int    mmio_read(off_t offset, void *buf, size_t size) = 0;
		virtual int    mmio_write(off_t offset, void const *buf, size_t size) = 0;
	};

	/**
	 * Initialize USB library
	 *
	 * \param tq  Timer_queue instance provided by the user of the library
	 * \param pd  Pci_device instance provided by the user of the library
	 * \param sr  Signal_receiver used by the library to install signals
	 *
	 * \return Pointer to Controller object that is used to access the xHCI device state
	 */
	Controller *usb_init(Timer_queue &tq, Pci_device &pd, Genode::Signal_receiver &sr);

	/**
	 * Reset USB libray
	 */
	void usb_reset();

	/**
	 * Update USB devices list
	 *
	 * Needs to be called after a library reset.
	 */
	void usb_update_devices();

	/**
	 * Library timer callback
	 *
	 * Needs to be called when a timer triggers (see Timer_queue interface).
	 *
	 * \param cb    Callback installed by Timer_queue::register_timer()
	 * \param data  Data pointer given in Timer_queue::register_timer()
	 */
	void usb_timer_callback(void (*cb)(void *data), void *data);
}

#endif /* _INCLUDE__QEMU__USB_H_ */
