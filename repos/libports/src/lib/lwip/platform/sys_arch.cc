/*
 * \brief  Platform dependent function implementations for lwIP.
 * \author Stefan Kalkowski
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/env.h>
#include <base/lock.h>
#include <base/sleep.h>
#include <parent/parent.h>
#include <os/timed_semaphore.h>

/* LwIP includes */
#include <lwip/genode.h>
#include <ring_buffer.h>
#include <thread.h>
#include <verbose.h>


namespace Lwip {

	class Mutex
	{
		public:
			Genode::Lock     lock;
			int              counter;
			Genode::Thread  *thread;

			Mutex() : counter(0), thread((Genode::Thread*)-1) {}
	};


	/* LwIP's so called mailboxes map to a ring buffer in our case */
	typedef Ring_buffer Mailbox;
}


/**
 * Returns lock used to synchronize with tcpip thread's startup.
 */
static Genode::Lock *startup_lock()
{
	static Genode::Lock _startup_lock(Genode::Lock::LOCKED);
	return &_startup_lock;
}


/**
 * Returns global mutex for lightweight protection mechanisms in LwIP.
 */
static Lwip::Mutex *global_mutex()
{
	static Lwip::Mutex _mutex;
	return &_mutex;
}


/**
 * Used for startup synchronization by the tcpip thread.
 */
static void startup_done(void *arg)
{
	startup_lock()->unlock();
}


using namespace Lwip;

extern "C" {

/* LwIP includes */
#include <lwip/opt.h>
#include <lwip/tcpip.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <lwip/dhcp.h>
#include <arch/sys_arch.h>
#include <arch/cc.h>
#include <nic.h>

/*
 * Sanitize check, whether somebody tries to use our LwIP backend,
 * without the tcpip thread and synchronization primitives,
 * we use LwIP multi-threaded anyway.
 */
#if NO_SYS
#error "You cannot use the Genode LwIP backend with NO_SYS!"
#endif //NO_SYS

#define netifapi_netif_set_link_up(n) netifapi_netif_common(n, netif_set_link_up, NULL)
#define netifapi_netif_set_link_down(n) netifapi_netif_common(n, netif_set_link_down, NULL)

	static struct netif netif;

	/*
	 * Callback function used when changing the interface state.
	 */
	static void status_callback(struct netif *netif)
	{
		static ip_addr_t ip_addr = { 0 };

		if (ip_addr.addr != netif->ip_addr.addr) {
			Genode::log("got IP address ",
			            ip4_addr1(&(netif->ip_addr)), ".",
			            ip4_addr2(&(netif->ip_addr)), ".",
			            ip4_addr3(&(netif->ip_addr)), ".",
			            ip4_addr4(&(netif->ip_addr)));
			ip_addr.addr = netif->ip_addr.addr;
		}
	}


	/*
	 * Callback function used when doing a link-state change.
	 */
	static void link_callback(struct netif *netif)
	{
		/*
		 * We call the status callback at this point to print
		 * the IP address because we may have got a new one,
		 * e.h., we joined another wireless network.
		 */
		status_callback(netif);
	}


	/********************
	 ** Initialization **
	 ********************/

	/**
	 * Function needed by LwIP.
	 */
	void sys_init(void) {}


	void lwip_tcpip_init()
	{
		/* call the tcpip initialization code and block, until it's initialized */
		tcpip_init(startup_done, 0);
		startup_lock()->lock();
	}


	/* in lwip/genode.h */
	int lwip_nic_init(Genode::int32_t ip_addr,
	                  Genode::int32_t netmask,
	                  Genode::int32_t gateway,
	                  Genode::size_t  tx_buf_size,
	                  Genode::size_t  rx_buf_size)
	{
		struct ip_addr ip, nm, gw;
		static struct netif_buf_sizes nbs;
		nbs.tx_buf_size = tx_buf_size;
		nbs.rx_buf_size = rx_buf_size;
		ip.addr = ip_addr;
		nm.addr = netmask;
		gw.addr = gateway;

		class Nic_not_availble { };

		try {
			/**
			 * Add Genode's nic, which uses the nic-session interface.
			 *
			 * LwIP recommends to use ethernet_input as packet pushing function
			 * for ethernet cards and ip_input for everything else.
			 * Nevertheless, when we use the tcpip synchronization subsystem,
			 * we should use tcpip_input anyway
			 *
			 * See: http://lwip.wikia.com/wiki/Writing_a_device_driver
			 */
			err_t ret = netifapi_netif_add(&netif, &ip, &nm, &gw, &nbs,
			                               genode_netif_init, tcpip_input);
			if (ret != ERR_OK)
				throw Nic_not_availble();

			/* Set Genode's nic as the default nic */
			netifapi_netif_set_default(&netif);

			/* If no static ip was set, we do dhcp */
			if (!ip_addr) {
#if LWIP_DHCP
				/* Register callback functions. */
				netif.status_callback = status_callback;
				netif.link_callback = link_callback;

				netifapi_dhcp_start(&netif);
#else
				/* no IP address - no networking */
				return 1;
#endif /* LWIP_DHCP */
			} else {
				netifapi_netif_set_up(&netif);
			}
		} catch (Nic_not_availble) {
			Genode::warning("NIC not available, loopback is used as default");
			return 2;
		}
		return 0;
	}


	void lwip_nic_link_state_changed(int state)
	{
		if (state)
			netifapi_netif_set_link_up(&netif);
		else
			netifapi_netif_set_link_down(&netif);
	}


	/***************
	 ** Semaphore **
	 ***************/

	/**
	 * Creates and returns a new semaphore.
	 *
	 * \param  count specifies the initial state of the semaphore.
	 * \return the semaphore, or SYS_SEM_NULL on error.
	 */
	err_t sys_sem_new(sys_sem_t* sem, u8_t count)
	{
		try {
			Genode::Timed_semaphore *_sem = new (Genode::env()->heap())
			                                Genode::Timed_semaphore(count);
			sem->ptr = _sem;
			return ERR_OK;
		} catch (Genode::Allocator::Out_of_memory) {
			Genode::warning(__func__, ": out of memory");
			return ERR_MEM;
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
			/* we just use a arbitrary value that is
			 * not defined in err.h */
			return -32;
		}
	}


	/**
	 * Frees a semaphore
	 *
	 * \param sem the semaphore to free
	 */
	void sys_sem_free(sys_sem_t* sem)
	{
		try {
			Genode::Timed_semaphore *_sem =
				reinterpret_cast<Genode::Timed_semaphore*>(sem->ptr);
			if (_sem)
				destroy(Genode::env()->heap(), _sem);
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
		}
	}


	/**
	 * Signals (or releases) a semaphore.
	 *
	 */
	void sys_sem_signal(sys_sem_t* sem)
	{
		try {
			Genode::Timed_semaphore *_sem =
				reinterpret_cast<Genode::Timed_semaphore*>(sem->ptr);
			if (!_sem) {
				return;
			}
			_sem->up();
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
		}
	}


	/**
	 * Checks if a semaphore is valid
	 *
	 * \param sem semaphore to check
	 *
	 * \return 1 if semaphore is valid, 0 otherwise.
	 */
	int sys_sem_valid(sys_sem_t* sem)
	{
		try {
			Genode::Timed_semaphore *_sem =
				reinterpret_cast<Genode::Timed_semaphore*>(sem->ptr);

			if (_sem)
				return 1;
		} catch (...) { }

		return 0;
	}


	/**
	 * Sets a semaphore to invalid
	 *
	 * \param sem semaphore to set invalid
	 */
	void sys_sem_set_invalid(sys_sem_t* sem)
	{
		sem->ptr = NULL;
	}

	/**
	 * Blocks the thread while waiting for the semaphore to be signaled.
	 *
	 * \param sem     semaphore used to block on
	 * \param timeout specifies how many milliseconds the function should block.
	 *                If timeout=0, then the function should block indefinitely.
	 * \return        SYS_ARCH_TIMEOUT if the function times out. If the function
	 *                acquires the semaphore, it should return how many
	 *                milliseconds expired while waiting for the semaphore.
	 */
	u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout)
	{
		using namespace Genode;
		try {
			Timed_semaphore *_sem = reinterpret_cast<Timed_semaphore*>(sem->ptr);
			if (!_sem) {
				return EINVAL;
			}

			/* If no timeout is requested, we have to track the time ourself */
			if (!timeout) {
				Alarm::Time starttime = Timeout_thread::alarm_timer()->time();
				_sem->down();
				return Timeout_thread::alarm_timer()->time() - starttime;
			} else {
				return _sem->down(timeout);
			}
		} catch (Timeout_exception) {
			return SYS_ARCH_TIMEOUT;
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
			return -1;
		}
	}


	/**
	 * Enter critical section
	 */
	sys_prot_t sys_arch_protect(void)
	{
		if (global_mutex()->thread == Genode::Thread::myself())
			return ++global_mutex()->counter;
		global_mutex()->lock.lock();
		global_mutex()->thread = Genode::Thread::myself();
		return 0;
	}


	/**
	 * Leave critical section.
	 */
	void sys_arch_unprotect(sys_prot_t pval)
	{
		if (global_mutex()->thread != Genode::Thread::myself())
			return;
		if (global_mutex()->counter > 1)
			global_mutex()->counter--;
		else {
			global_mutex()->counter = 0;
			global_mutex()->thread = (Genode::Thread*)-1;
			global_mutex()->lock.unlock();
		}
	}


	/***************
	 ** Mailboxes **
	 ***************/

	/**
	 * Allocate a new mailbox.
	 *
	 * \param size  size of the mailbox
	 * \return      a new mailbox, or SYS_MBOX_NULL on error.
	 */
	err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
		LWIP_UNUSED_ARG(size);
		try {
			Mailbox* _mbox = new (Genode::env()->heap()) Mailbox();
			mbox->ptr = _mbox;
			return ERR_OK;
		} catch (Genode::Allocator::Out_of_memory) {
			Genode::warning(__func__, ": out of memory");
			return ERR_MEM;
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
			return -32;
		}
	}


	/**
	 * Free a mailbox.
	 *
	 * \param mbox  mailbox to free
	 */
	void sys_mbox_free(sys_mbox_t* mbox)
	{
		try {
			Mailbox* _mbox = reinterpret_cast<Mailbox*>(mbox->ptr);
			if (_mbox)
				destroy(Genode::env()->heap(), _mbox);
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
		}
	}

	/**
	 * Checks if a mailbox is valid.
	 *
	 * \param mbox mailbox to check
	 *
	 * \return 1 if mailbox is valid, 0 otherwise.
	 */
	int sys_mbox_valid(sys_mbox_t* mbox)
	{
		try {
			Mailbox* _mbox = reinterpret_cast<Mailbox*>(mbox->ptr);
			if (_mbox) {
				return 1;
			}
		} catch (...) { }

		return 0;
	}


	/**
	 * Invalidate a mailbox
	 *
	 * Afterwards sys_mbox_valid() returns 0.
	 * ATTENTION: This does NOT mean that the mailbox shall be deallocated:
	 * sys_mbox_free() is always called before calling this function!
	 *
	 * \param mbox mailbox to set invalid
	 */
	void sys_mbox_set_invalid(sys_mbox_t* mbox)
	{
		mbox->ptr = NULL;
	}


	/**
	 * Posts the "msg" to the mailbox.
	 *
	 * \param mbox target mailbox
	 * \param msg  message to post
	 */
	void sys_mbox_post(sys_mbox_t* mbox, void *msg)
	{
		while (true) {
			try {
				Mailbox* _mbox = reinterpret_cast<Mailbox*>(mbox->ptr);
				if (!_mbox) {
					return;
				}
				_mbox->add(msg);
				return;
			} catch (Mailbox::Overflow) {
				if (verbose)
					Genode::warning(__func__, ": overflow exception!");
			} catch (...) {
				Genode::error(__func__, ": unknown exception occured!");
			}
		}
	}


	/**
	 * Posts the "msg" to the mailbox, returns when it is full.
	 *
	 * \param mbox target mailbox
	 * \param msg  message to post
	 */
	err_t sys_mbox_trypost(sys_mbox_t* mbox, void *msg)
	{
		try {
			Mailbox* _mbox = reinterpret_cast<Mailbox*>(mbox->ptr);
			if (!_mbox) {
				return EINVAL;
			}
			_mbox->add(msg);
			return ERR_OK;
		} catch (Mailbox::Overflow) {
			if (verbose)
				Genode::warning(__func__, ": overflow exception!");
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
		}
		return ERR_MEM;
	}


	/**
	 * Fetch a "msg" from the mailbox.
	 *
	 * \param mbox     target mailbox
	 * \param msg      pointer to message buffer
	 * \param timeout  how long will it block, if no message is available
	 */
	u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void **msg, u32_t timeout)
	{
		/*
		 * The mailbox might be invalid to indicate,
		 * that the message should be dropped
		 */
		if (!mbox)
			return 0;

		u32_t ret = 0;

		try {
			Mailbox* _mbox = reinterpret_cast<Mailbox *>(mbox->ptr);
			if (!_mbox)
				return 0;
			ret = _mbox->get(msg, timeout);
		} catch (Genode::Timeout_exception) {
			ret = SYS_ARCH_TIMEOUT;
		} catch (Genode::Nonblocking_exception) {
			ret = SYS_MBOX_EMPTY;
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
			ret = SYS_ARCH_TIMEOUT;
		}

		return ret;
	}


	/**
	 * Try to fetch a "msg" from the mailbox.
	 *
	 * \param mbox     target mailbox
	 * \param msg      pointer to message buffer
	 * \return on success 0 is returned.
	 *
	 * This is similar to sys_arch_mbox_fetch, however if a message is not present
	 * in the mailbox, it immediately returns with the code SYS_MBOX_EMPTY.
	 */
	u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void **msg)
	{
		return sys_arch_mbox_fetch(mbox, msg, Mailbox::NO_BLOCK);
	}


	/*************
	 ** Threads **
	 *************/

	/**
	 * Create and run a new thread.
	 *
	 * \param name       name of the new thread
	 * \param thread     pointer to thread's entry function
	 * \param arg        arguments to the entry function
	 * \param stacksize  size of the thread's stack
	 * \param prio       priority of new thread
	 */
	sys_thread_t sys_thread_new(const char *name, void (* thread)(void *arg),
	                            void *arg, int stacksize, int prio)
	{
		try
		{
			LWIP_UNUSED_ARG(stacksize);
			LWIP_UNUSED_ARG(prio);
			Lwip_thread *_th = new (Genode::env()->heap())
				Lwip_thread(name, thread, arg);
			_th->start();
			return (sys_thread_t) _th;
		} catch (Genode::Allocator::Out_of_memory) {
			Genode::warning(__func__, ": out of memory");
			return 0;
		} catch (...) {
			Genode::error(__func__, ": unknown Exception occured!");
			return ERR_MEM;
		}
	}

	u32_t sys_now() {
		return Genode::Timeout_thread::alarm_timer()->time(); }

#if 0
	/**************
	 ** Timeouts **
	 **************/

	/**
	 * Returns linked list of timeout for a thread created within LwIP
	 * (by now this is only the tcpip thread)
	 */
	struct sys_timeouts *sys_arch_timeouts(void)
	{
		using namespace Genode;

		struct Thread_timeout
		{
			sys_timeouts    timeouts;
			Thread         *thread;
			Thread_timeout *next;

			Thread_timeout(Thread *t = 0)
			: thread(t), next(0) { timeouts.next = 0; }
		};
		static Lock mutex;
		static Thread_timeout thread_timeouts;

		Lock::Guard lock_guard(mutex);

		try {
			Thread *thread = Thread::myself();

			/* check available timeout heads */
			for (Thread_timeout *tt = &thread_timeouts; tt; tt = tt->next)
				if (tt->thread == thread)
					return &tt->timeouts;

			/* we have to add a new one */
			Thread_timeout *tt   = new (env()->heap()) Thread_timeout(thread);
			tt->next             = thread_timeouts.next;
			thread_timeouts.next = tt;

			return &tt->timeouts;
		} catch (...) {
			Genode::error(__func__, ": unknown exception occured!");
			return 0;
		}
	}
#endif

	void genode_memcpy(void * dst, const void *src, unsigned long size) {
		Genode::memcpy(dst, src, size);
	}
} /* extern "C" */


extern "C" void lwip_sleep_forever()
{
	int dummy = 1;
	Genode::log(__func__, ": thread ", (unsigned)(((Genode::addr_t)&dummy)>>20)&0xff);
	Genode::sleep_forever();
}
