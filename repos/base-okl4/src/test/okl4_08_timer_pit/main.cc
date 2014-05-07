/*
 * \brief  Test for interrupt handling and timer on OKL4
 * \author Norman Feske
 * \date   2009-03-31
 *
 * This program can be started as roottask replacement directly on the OKL4
 * kernel. It has two purposes, to test the interrupt handling on OKL4 and to
 * provide a user-level time source. The x86 version of the OKL4 kernel uses
 * the APIC timer as scheduling timer. So the PIT free to use as user-land time
 * source. This is needed because the OKL4 kernel provides no means to access
 * the kernel-level time source through IPC timeouts anymore.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* local includes */
#include "../mini_env.h"
#include "../io_port.h"

namespace Okl4 { extern "C" {
#include <l4/thread.h>
#include <l4/interrupt.h>
#include <l4/security.h>
#include <l4/ipc.h>
} }

using namespace Okl4;
using namespace Genode;

enum { IRQ_PIT = 0 };  /* timer interrupt line at the PIC */


enum {
	PIT_TICKS_PER_SECOND = 1193182,
	PIT_MAX_COUNT        =   65535,
	PIT_DATA_PORT_0      =    0x40,  /* data port for PIT channel 0, connected
	                                    to the PIC */
	PIT_CMD_PORT         =    0x43   /* PIT command port */
};


/**
 * Bit definitions for accessing the PIT command port
 */
enum {
	PIT_CMD_SELECT_CHANNEL_0 = 0 << 6,
	PIT_CMD_ACCESS_LO        = 1 << 4,
	PIT_CMD_ACCESS_LO_HI     = 3 << 4,
	PIT_CMD_MODE_IRQ         = 0 << 1,
	PIT_CMD_MODE_RATE        = 2 << 1,
};


/**
 * Set PIT counter value
 */
static inline void pit_set_counter(uint16_t value)
{
	outb(PIT_DATA_PORT_0, value & 0xff);
	outb(PIT_DATA_PORT_0, (value >> 8) & 0xff);
}


/**
 * Main program
 */
int main()
{
	/* operate PIT in one-shot mode */
	outb(PIT_CMD_PORT, PIT_CMD_SELECT_CHANNEL_0 |
	                   PIT_CMD_ACCESS_LO_HI |
	                   PIT_CMD_MODE_IRQ);

	int irq = IRQ_PIT;

	/* allow roottask (ourself) to handle the interrupt */
	L4_LoadMR(0, irq);
	int ret = L4_AllowInterruptControl(L4_rootspace);
	if (ret != 1)
		printf("L4_AllowInterruptControl returned %d, error code=%ld\n",
		       ret, L4_ErrorCode());

	/* bit to use for IRQ notifications */
	enum { IRQ_NOTIFY_BIT = 13 };

	/*
	 * Note: 'L4_Myself()' does not work for the thread argument of
	 *       'L4_RegisterInterrupt'. We have to specify our global ID.
	 */
	L4_LoadMR(0, irq);
	ret = L4_RegisterInterrupt(L4_rootserver, IRQ_NOTIFY_BIT, 0, 0);
	if (ret != 1)
		printf("L4_RegisterInterrupt returned %d, error code=%ld\n",
		        ret, L4_ErrorCode());

	/* prepare ourself to receive asynchronous IRQ notifications */
	L4_ThreadId_t partner = L4_nilthread;
	L4_Set_NotifyMask(1 << IRQ_NOTIFY_BIT);
	L4_Accept(L4_NotifyMsgAcceptor);

	int cnt = 0, seconds = 1;
	for (;;) {
		/* wait for asynchronous interrupt notification */
		L4_ReplyWait(partner, &partner);

		/*
		 * Schedule next interrupt
		 *
		 * The PIT generates the next interrupt when reaching
		 * PIT_MAX_COUNT. By initializing the PIT with a higher
		 * value than 0, we can shorten the time until the next
		 * interrupt occurs.
		 */
		pit_set_counter(0);

		/* we got an interrupt, acknowledge */
		L4_LoadMR(0, irq);
		L4_AcknowledgeInterrupt(0, 0);

		/* count timer interrupts, print a message each second */
		if (cnt++ == PIT_TICKS_PER_SECOND/PIT_MAX_COUNT) {
			printf("Second %d\n", seconds++);
			cnt = 0;
		}
	}
	return 0;
}
