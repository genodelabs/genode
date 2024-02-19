/*
 * \brief Qemu USB controller interface
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date 2015-12-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__QEMU__USB_H_
#define _INCLUDE__QEMU__USB_H_

#include <base/stdint.h>
#include <base/signal.h>
#include <base/allocator.h>

namespace Qemu {

	typedef Genode::size_t   size_t;
	typedef Genode::off_t    off_t;
	typedef Genode::int64_t  int64_t;
	typedef Genode::addr_t   addr_t;
	typedef Genode::uint16_t uint16_t;


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
		enum class Dma_direction { IN = 0, OUT = 1, };

		/**
		 * Raise interrupt
		 *
		 * \param assert 1 == raise, 0 == deassert interrupt
		 */
		virtual void  raise_interrupt(int assert) = 0;
		virtual int   read_dma(addr_t addr, void *buf, size_t size) = 0;
		virtual int   write_dma(addr_t addr, void const *buf, size_t size) = 0;
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
		/*
		 * Controller information
		 */
		struct Info
		{
			uint16_t vendor_id;
			uint16_t product_id;
		};

		/**
		 * Get information of the controller
		 */
		virtual Info info() const = 0;

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
	 * \param ep  Entrypoint used by the library to install signals
	 *
	 * \return Pointer to Controller object that is used to access the xHCI device state
	 */
	Controller *usb_init(Timer_queue &tq, Pci_device &pd,
	                     Genode::Entrypoint &ep,
	                     Genode::Allocator &, Genode::Env &,
	                     Genode::Xml_node const &);

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
