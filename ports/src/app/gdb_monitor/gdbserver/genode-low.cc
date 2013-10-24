/*
 * \brief  Genode backend for GDBServer (C++)
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {
#define private _private
#include "genode-low.h"
#include "linux-low.h"
#define _private private

int linux_detach_one_lwp (struct inferior_list_entry *entry, void *args);
}

#include <base/printf.h>
#include <dataspace/client.h>

#include "cpu_session_component.h"

#include "gdb_stub_thread.h"

static bool verbose = false;

using namespace Genode;
using namespace Gdb_monitor;


static Lock &main_thread_ready_lock()
{
	static Lock _main_thread_ready_lock(Lock::LOCKED);
	return _main_thread_ready_lock;
}


static Lock &gdbserver_ready_lock()
{
	static Lock _gdbserver_ready_lock(Lock::LOCKED);
	return _gdbserver_ready_lock;
}


Gdb_stub_thread *gdb_stub_thread()
{
	return (Gdb_stub_thread*)(current_process()->_private->gdb_stub_thread);
}


extern "C" int genode_signal_fd()
{
	return gdb_stub_thread()->signal_fd();
}


void genode_add_thread(unsigned long lwpid)
{
	if (lwpid == GENODE_LWP_BASE) {
		main_thread_ready_lock().unlock();
	} else {
		if (lwpid == GENODE_LWP_BASE + 1) {
			/* make sure gdbserver is ready to attach new threads */
			gdbserver_ready_lock().lock();
		}
		linux_attach_lwp(lwpid);
	}
}


void genode_remove_thread(unsigned long lwpid)
{
	int pid = GENODE_LWP_BASE;
	linux_detach_one_lwp((struct inferior_list_entry *)
		find_thread_ptid(ptid_build(GENODE_LWP_BASE, lwpid, 0)), &pid);
}


void genode_wait_for_target_main_thread()
{
	/* gdbserver is now ready to attach new threads */
	gdbserver_ready_lock().unlock();

	/* wait until the target's main thread has been created */
	main_thread_ready_lock().lock();
}


extern "C" void genode_detect_all_threads()
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->next(csc->first()); /* second thread */

	while (thread_cap.valid()) {
		linux_attach_lwp(csc->lwpid(thread_cap));
		thread_cap = csc->next(thread_cap);
	}
}


extern "C" void genode_stop_all_threads()
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->first();

	while (thread_cap.valid()) {
		csc->pause(thread_cap);
		thread_cap = csc->next(thread_cap);
	}
}


extern "C" void genode_resume_all_threads()
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->first();

	while (thread_cap.valid()) {
		csc->resume(thread_cap);
		thread_cap = csc->next(thread_cap);
	}
}


int genode_detach(int pid)
{
    find_inferior (&all_threads, linux_detach_one_lwp, &pid);

    genode_resume_all_threads();

    return 0;
}


int genode_kill(int pid)
{
    /* TODO */
    if (verbose) PDBG("genode_kill() called - not implemented\n");

    return genode_detach(pid);
}


void genode_interrupt_thread(unsigned long lwpid)
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->thread_cap(lwpid);

	if (!thread_cap.valid()) {
		PERR("could not find thread capability for lwpid %lu", lwpid);
		return;
	}

	csc->pause(thread_cap);
}


void genode_continue_thread(unsigned long lwpid, int single_step)
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->thread_cap(lwpid);

	if (!thread_cap.valid()) {
		PERR("could not find thread capability for lwpid %lu", lwpid);
		return;
	}

	csc->single_step(thread_cap, single_step);
	csc->resume(thread_cap);
}


unsigned long genode_find_segfault_lwpid()
{
	Cpu_session_component *csc = gdb_stub_thread()->cpu_session_component();

	Thread_capability thread_cap = csc->first();

	while (thread_cap.valid()) {
		Thread_state thread_state = csc->state(thread_cap);
		if (thread_state.unresolved_page_fault)
			return csc->lwpid(thread_cap);
		thread_cap = csc->next(thread_cap);
	}

	PDBG("could not determine thread which caused the page fault");
	return 1;
}


class Memory_model
{
	private:

		Lock _lock;

		Rm_session_component * const _address_space;

		/**
		 * Representation of a currently mapped region
		 */
		struct Mapped_region
		{
			Rm_session_component::Region *_region;
			unsigned char                *_local_base;

			Mapped_region() : _region(0), _local_base(0) { }

			bool valid() { return _region != 0; }

			bool is_loaded(Rm_session_component::Region const * region)
			{
				return _region == region;
			}

			void flush()
			{
				if (!valid()) return;
				env()->rm_session()->detach(_local_base);
				_local_base = 0;
				_region = 0;
			}

			void load(Rm_session_component::Region *region)
			{
				if (region == _region)
					return;

				if (!region || valid())
					flush();

				if (!region)
					return;

				try {
					_region     = region;
					_local_base = env()->rm_session()->attach(_region->ds_cap(),
					                                          0, _region->offset());
				} catch (Rm_session::Attach_failed) {
					flush();
					PERR("Memory_model: RM attach failed");
				}
			}

			unsigned char *local_base() { return _local_base; }
		};

		enum { NUM_MAPPED_REGIONS = 1 };

		Mapped_region _mapped_region[NUM_MAPPED_REGIONS];

		unsigned _evict_idx;

		/**
		 * Return local address of mapped region
		 *
		 * The function returns 0 if the mapping fails
		 */
		unsigned char *_update_curr_region(Rm_session_component::Region *region)
		{
			for (unsigned i = 0; i < NUM_MAPPED_REGIONS; i++) {
				if (_mapped_region[i].is_loaded(region))
					return _mapped_region[i].local_base();
			}

			/* flush one currently mapped region */
			_evict_idx++;
			if (_evict_idx == NUM_MAPPED_REGIONS)
				_evict_idx = 0;

			_mapped_region[_evict_idx].load(region);

			return _mapped_region[_evict_idx].local_base();
		}

	public:

		Memory_model(Rm_session_component *address_space)
		:
			_address_space(address_space), _evict_idx(0)
		{ }

		unsigned char read(void *addr)
		{
			Lock::Guard guard(_lock);

			addr_t offset_in_region = 0;

			Rm_session_component::Region *region =
				_address_space->find_region(addr, &offset_in_region);

			unsigned char *local_base = _update_curr_region(region);

			if (!local_base) {
				PWRN("Memory model: no memory at address %p", addr);
				return 0;
			}

			unsigned char value =
				local_base[offset_in_region];

			if (verbose)
				Genode::printf("read addr=%p, value=%x\n", addr, value);

			return value;
		}

		void write(void *addr, unsigned char value)
		{
			if (verbose)
			Genode::printf("write addr=%p, value=%x\n", addr, value);

			Lock::Guard guard(_lock);

			addr_t offset_in_region = 0;
			Rm_session_component::Region *region =
				_address_space->find_region(addr, &offset_in_region);

			unsigned char *local_base = _update_curr_region(region);

			if (!local_base) {
				PWRN("Memory model: no memory at address %p", addr);
				PWRN("(attempted to write %x)", (int)value);
				return;
			}

			local_base[offset_in_region] = value;
		}
};


/**
 * Return singleton instance of memory model
 */
static Memory_model *memory_model()
{
	static Memory_model inst(gdb_stub_thread()->rm_session_component());
	return &inst;
}


unsigned char genode_read_memory_byte(void *addr)
{
	return memory_model()->read(addr);
}


void genode_write_memory_byte(void *addr, unsigned char value)
{
	return memory_model()->write(addr, value);
}
