/*
 * \brief  Launchpad child management
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2006-09-01
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Copyright (C) 2012 Intel Corporation    
 * 
 * Modifications are contributed under the terms and conditions of the
 * Genode Contributor's Agreement executed by Intel.
 */

#include <base/env.h>
#include <base/child.h>
#include <base/sleep.h>
#include <base/service.h>
#include <base/snprintf.h>
#include <base/blocking.h>
#include <rom_session/connection.h>
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <timer_session/connection.h>
#include <launchpad/launchpad.h>

using namespace Genode;


/***************
 ** Launchpad **
 ***************/

Launchpad::Launchpad(unsigned long initial_quota)
:
	_initial_quota(initial_quota),
	_sliced_heap(env()->ram_session(), env()->rm_session())
{
	/* names of services provided by the parent */
	static const char *names[] = {

		/* core services */
		"CAP", "RAM", "RM", "PD", "CPU", "IO_MEM", "IO_PORT",
		"IRQ", "ROM", "LOG", "SIGNAL",

		/* services expected to got started by init */
		"Nitpicker", "Init", "Timer", "PCI", "Block", "Nic", "Rtc",
		
		0 /* null-termination */
	};
	for (unsigned i = 0; names[i]; i++)
		_parent_services.insert(new (env()->heap())
		                            Parent_service(names[i]));
}


/**
 * Check if a program with the specified name already exists
 */
bool Launchpad::_child_name_exists(const char *name)
{
	Launchpad_child *c = _children.first();

	for ( ; c; c = c->List<Launchpad_child>::Element::next())
		if (strcmp(c->name(), name) == 0)
			return true;

	return false;
}


/**
 * Create a unique name based on the filename
 *
 * If a program with the filename as name already exists, we
 * add a counting number as suffix.
 */
void Launchpad::_get_unique_child_name(const char *filename, char *dst, int dst_len)
{
	Lock::Guard lock_guard(_children_lock);

	char buf[64];
	char suffix[8];
	suffix[0] = 0;

	for (int cnt = 1; true; cnt++) {

		/* build program name composed of filename and numeric suffix */
		snprintf(buf, sizeof(buf), "%s%s", filename, suffix);

		/* if such a program name does not exist yet, we are happy */
		if (!_child_name_exists(buf)) {
			strncpy(dst, buf, dst_len);
			return;
		}

		/* increase number of suffix */
		snprintf(suffix, sizeof(suffix), ".%d", cnt + 1);
	}
}


Launchpad_child *Launchpad::start_child(const char *filename,
                                        unsigned long ram_quota,
                                        Genode::Dataspace_capability config_ds)
{
	printf("starting %s with quota %ld\n", filename, ram_quota);

	/* find unique name for new child */
	char unique_name[64];
	_get_unique_child_name(filename, unique_name, sizeof(unique_name));
	printf("using unique child name \"%s\"\n", unique_name);

	if (ram_quota > env()->ram_session()->avail()) {
		PERR("Child's ram quota is higher than our available quota, using available quota");
		ram_quota = env()->ram_session()->avail() - 256*1000;
	}

	size_t metadata_size = 4096*16 + sizeof(Launchpad_child);

	if (metadata_size > ram_quota) {
		PERR("Too low ram_quota to hold child metadata");
		return 0;
	}

	ram_quota -= metadata_size;

	/* lookup executable elf binary */
	Dataspace_capability  file_cap;
	Rom_session_capability rom_cap;
	try {
		/*
		 * When creating a ROM connection for a non-existing file, the
		 * constructor of 'Rom_connection' throws a 'Parent::Service_denied'
		 * exception.
		 */
		Rom_connection rom(filename, unique_name);
		rom.on_destruction(Rom_connection::KEEP_OPEN);
		rom_cap  = rom.cap();
		file_cap = rom.dataspace();
	} catch (...) {
		printf("Error: Could not access file \"%s\" from ROM service.\n", filename);
		return 0;
	}

	/* create ram session for child with some of our own quota */
	Ram_connection ram;
	ram.on_destruction(Ram_connection::KEEP_OPEN);
	ram.ref_account(env()->ram_session_cap());
	env()->ram_session()->transfer_quota(ram.cap(), ram_quota);

	/* create cpu session for child */
	Cpu_connection cpu(unique_name);
	cpu.on_destruction(Cpu_connection::KEEP_OPEN);

	if (!ram.cap().valid() || !cpu.cap().valid()) {
		if (ram.cap().valid()) {
			PWRN("Failed to create CPU session");
			env()->parent()->close(ram.cap());
		}
		if (cpu.cap().valid()) {
			PWRN("Failed to create RAM session");
			env()->parent()->close(cpu.cap());
		}
		env()->parent()->close(rom_cap);
		PERR("Our quota is %zd", env()->ram_session()->quota());
		return 0;
	}

	Rm_connection rm;
	rm.on_destruction(Rm_connection::KEEP_OPEN);
	if (!rm.cap().valid()) {
		PWRN("Failed to create RM session");
		env()->parent()->close(ram.cap());
		env()->parent()->close(cpu.cap());
		env()->parent()->close(rom_cap);
		return 0;
	}

	Launchpad_child *c = new (&_sliced_heap)
		Launchpad_child(unique_name, file_cap, ram.cap(),
		                cpu.cap(), rm.cap(), rom_cap,
		                &_cap_session, &_parent_services, &_child_services,
		                config_ds, this);

	Lock::Guard lock_guard(_children_lock);
	_children.insert(c);

	add_child(unique_name, ram_quota, c, c->heap());
	return c;
}


/**
 * Watchdog-guarded child destruction mechanism
 *
 * During the destruction of a child, all sessions of the child are getting
 * closed. A server, however, may refuse to answer a close call. We detect
 * this case using a watchdog mechanism, unblock the 'close' call, and
 * proceed with the closing the other remaining sessions.
 */
class Child_destructor_thread : Thread<2*4096>
{
	private:

		Launchpad_child *_curr_child;     /* currently destructed child       */
		Allocator       *_curr_alloc;     /* child object'sallocator          */
		Lock             _submit_lock;    /* only one submission at a time    */
		Lock             _activate_lock;  /* submission protocol              */
		bool             _ready;          /* set if submission is completed   */
		int              _watchdog_cnt;   /* watchdog counter in milliseconds */

		/**
		 * Thread entry function
		 */
		void entry() {
			while (true) {

				/* wait for next submission */
				_activate_lock.lock();

				/*
				 * Eventually long-taking operation that involves the
				 * closing of all session of the child. This procedure
				 * may need blocking cancellation to proceed in the
				 * case servers are unresponsive.
				 */
				try {
					destroy(_curr_alloc, _curr_child);
				} catch (Blocking_canceled) {
					PERR("Suspicious cancellation\n");
				}

				_ready = true;
			}
		}

	public:

		/*
		 * Watchdog timer granularity in milliseconds. This value defined
		 * after how many milliseconds the watchdog is activated.
		 */
		enum { WATCHDOG_GRANULARITY_MS = 10 };

		/**
		 * Constructor
		 */
		Child_destructor_thread() :
			_curr_child(0), _curr_alloc(0),
			_activate_lock(Lock::LOCKED),
			_ready(true)
		{
			start();
		}

		/**
		 * Destruct child, coping with unresponsive servers
		 *
		 * \param alloc       Child object's allocator
		 * \param child       Child to destruct
		 * \param timeout_ms  Maximum destruction time until the destructing
		 *                    thread gets waken up to give up the close call to
		 *                    an unreponsive server.
		 */
		void submit_for_destruction(Allocator *alloc, Launchpad_child *child,
		                            Timer::Session *timer, int timeout_ms)
		{
			/* block until destructor thread is ready for new submission */
			Lock::Guard _lock_guard(_submit_lock);

			/* register submission values */
			_curr_child   = child;
			_curr_alloc   = alloc;
			_ready        = false;
			_watchdog_cnt = 0;

			/* wake up the destruction thread */
			_activate_lock.unlock();

			/*
			 * Now, the destruction thread attempts to close all the
			 * child's sessions. Check '_ready' flag periodically.
			 */
			while (!_ready) {

				/* give the destruction thread some time to proceed */
				timer->msleep(WATCHDOG_GRANULARITY_MS);
				_watchdog_cnt += WATCHDOG_GRANULARITY_MS;

				/* check if we reached the timeout */
				if (_watchdog_cnt > timeout_ms) {

					/*
					 * The destruction seems to got stuck, let's shake it a
					 * bit to proceed and reset the watchdog counter to give
					 * the next blocking operation a chance to execute.
					 */
					cancel_blocking();
					_watchdog_cnt = 0;
				}
			}
		}
};


/**
 * Construct a timer session for the watchdog timer on demand
 */
static Timer::Session *timer_session()
{
	static Timer::Connection timer;
	return &timer;
}


/**
 * Destruct Launchpad_child, cope with infinitely blocking server->close calls
 *
 * The arguments correspond to the 'Child_destructor_thread::submit_for_destruction'
 * function.
 */
static void destruct_child(Allocator *alloc, Launchpad_child *child,
                           Timer::Session *timer, int timeout)
{
	/* lazily construct child-destructor thread */
	static Child_destructor_thread child_destructor;

	/* if no timer session was provided by our caller, we have create one */
	if (!timer)
		timer = timer_session();

	child_destructor.submit_for_destruction(alloc, child, timer, timeout);
}


void Launchpad::exit_child(Launchpad_child *child,
                           Timer::Session  *timer,
                           int              session_close_timeout_ms)
{
	remove_child(child->name(), child->heap());

	Lock::Guard lock_guard(_children_lock);
	_children.remove(child);

	Rm_session_capability   rm_session_cap = child->rm_session_cap();
	Ram_session_capability ram_session_cap = child->ram_session_cap();
	Cpu_session_capability cpu_session_cap = child->cpu_session_cap();
	Rom_session_capability rom_session_cap = child->rom_session_cap();

	const Genode::Server *server = child->server();
	destruct_child(&_sliced_heap, child, timer, session_close_timeout_ms);

	env()->parent()->close(rm_session_cap);
	env()->parent()->close(cpu_session_cap);
	env()->parent()->close(rom_session_cap);
	env()->parent()->close(ram_session_cap);

	/*
	 * The killed child may have provided services to other children.
	 * Since the server is dead by now, we cannot close its sessions
	 * in the cooperative way. Instead, we need to instruct each
	 * other child to forget about session associated with the dead
	 * server. Note that the 'child' pointer points a a no-more
	 * existing object. It is only used to identify the corresponding
	 * session. It must never by de-referenced!
	 */
	Launchpad_child *c = _children.first();
	for ( ; c; c = c->Genode::List<Launchpad_child>::Element::next())
		c->revoke_server(server);
}
