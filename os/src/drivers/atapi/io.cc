/*
 * \brief  I/O back-end of the mindrvr driver
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <dataspace/client.h>
#include <io_port_session/connection.h>
#include <irq_session/connection.h>
#include <timer_session/connection.h>

#include "ata_device.h"
#include "ata_bus_master.h"
#include "mindrvr.h"
#include "pio.h"

using namespace Genode;
using namespace Ata;

namespace Ata {

	/**
	 * Alarm thread, which counts jiffies and triggers timeout events.
	 */
	class Timeout_thread : public Thread<4096>
	{
		private:

			Timer::Connection _timer;   /* timer session */
			long              _jiffies; /* jiffies counter */

			void entry(void);

		public:

			enum { GRANULARITY_MSECS = 1000 };

			Timeout_thread()
			: Thread<4096>("jiffies"), _jiffies(0)
			{
				start();
			}

			long time(void) { return _jiffies; }

			/*
			 * Returns the singleton timeout-thread used for all timeouts.
			 */
			static Timeout_thread *timer();
	};


	void Timeout_thread::entry()
	{
		while (true) {
			_timer.msleep(GRANULARITY_MSECS);
			_jiffies += 1;

			if (_jiffies < 0)
				_jiffies = 0;
		}
	}


	Timeout_thread *Timeout_thread::timer()
	{
		static Timeout_thread _timer;
		return &_timer;
	}

}


/*********************************
 ** Mindrvr back-end C-bindings **
 *********************************/

namespace Ata {

	/*
	 * Friends of Device class
	 */
	inline Genode::Io_port_session * get_io(Device *dev)
	{
		return dev->io();
	}


	inline Bus_master * get_bus_master(Device *dev)
	{
		return dev->bus_master();
	}


	inline Genode::Irq_connection * get_irq(Device *dev)
	{
		return dev->irq();
	}

}

extern "C" unsigned char pio_inbyte(unsigned char addr)
{
	return get_io(Device::current())->inb(addr);
}


extern "C" unsigned int pio_inword(unsigned char addr)
{
	return get_io(Device::current())->inw(addr);
}


extern "C" unsigned long pio_indword(unsigned char addr)
{
	return get_io(Device::current())->inl(addr);
}


extern "C" void pio_outbyte(int addr, unsigned char data)
{
	get_io(Device::current())->outb(addr, data);
}


extern "C" void pio_outword(int addr, unsigned int data)
{
	get_io(Device::current())->outw(addr, data);
}


extern "C" void pio_outdword(int addr, unsigned long data)
{
	get_io(Device::current())->outl(addr, data);
}


extern "C" unsigned char pio_readBusMstrCmd(void)
{
	if (!get_bus_master(Device::current())) return 0;
	return get_bus_master(Device::current())->read_cmd();
}


extern "C" unsigned char pio_readBusMstrStatus(void)
{
	if (!get_bus_master(Device::current())) return 0;
	return get_bus_master(Device::current())->read_status();
}


extern "C" void pio_writeBusMstrCmd(unsigned char x)
{
	if (!get_bus_master(Device::current())) return;
	get_bus_master(Device::current())->write_cmd(x);
}


extern "C" void pio_writeBusMstrStatus(unsigned char x)
{
	if (!get_bus_master(Device::current())) return;
	get_bus_master(Device::current())->write_status(x);
}


extern "C" void pio_writeBusMstrPrd(unsigned long x)
{
	if (!get_bus_master(Device::current())) return;
	get_bus_master(Device::current())->write_prd(x);
}


extern "C" long SYSTEM_READ_TIMER(void)
{
	return Ata::Timeout_thread::timer()->time();
}


/* We just wait for interrupts */
extern "C" int SYSTEM_WAIT_INTR_OR_TIMEOUT(void)
{
	get_irq(Device::current())->wait_for_irq();
	int_ata_status   = pio_inbyte(CB_STAT);
	int_bmide_status = get_bus_master(Device::current())->read_status();
	return 0;
}


extern "C" int printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	Genode::vprintf(format, args);
	va_end(args);
	return 0;
}


/**
 * Configure/setup for Read/Write DMA
 *
 * The caller must call this function before attempting to use any ATA or ATAPI
 * commands in PCI DMA mode.
 *
 * MINDRVR assumes the entire DMA data transfer is contained within a single
 * contiguous I/O buffer. You may not need the 'dma_pci_config()' function
 * depending on how your system allocates the PRD buffer.
 *
 * This function shows an example of PRD buffer allocation. The PRD buffer
 * must be aligned on a 8 byte boundary and must not cross a 64K byte boundary
 * in memory.
 */
extern "C" int dma_pci_config( void )
{
	extern unsigned char *prdBuf;
	extern unsigned long *prdBufPtr;
	extern unsigned char  statReg;
	extern unsigned long *dma_pci_prd_ptr;
	extern int            dma_pci_num_prd;

	/*
	 * Set up the PRD entry list buffer address - the PRD entry list may not
	 * span a 64KB boundary in physical memory. Space is allocated (above) for
	 * this buffer such that it will be aligned on a seqment boundary and such
	 * that the PRD list will not span a 64KB boundary...
	 * ... return the address of the first PRD
	 */

	Dataspace_capability ds_cap = env()->ram_session()->alloc(PRD_BUF_SIZE);
	unsigned long *prd_addr = env()->rm_session()->attach(ds_cap);
	Dataspace_client ds_client(ds_cap);

	Genode::printf("PRD base at %08lx (physical) at %08lx (virtual)\n",
	               ds_client.phys_addr(), (long)prd_addr);

	dma_pci_prd_ptr = prdBufPtr = prd_addr;
	prdBuf = (unsigned char*)prd_addr;

	/* ... return the current number of PRD entries */
	dma_pci_num_prd = 0;

	/* read the BM status reg and save the upper 3 bits */
	statReg = (unsigned char) ( pio_readBusMstrStatus() & 0x60 );

	get_bus_master(Device::current())->set_prd((unsigned long)prd_addr,
	                                           (unsigned long)ds_client.phys_addr());
	return 0;
}
