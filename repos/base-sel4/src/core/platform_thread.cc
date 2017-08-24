/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/string.h>

/* core includes */
#include <platform_thread.h>
#include <platform_pd.h>

/* base-internal includes */
#include <base/internal/capability_space_sel4.h>
#include <base/internal/native_utcb.h>

/* seL4 includes */
#include <sel4/benchmark_utilisation_types.h>

using namespace Genode;


/*****************************************************
 ** Implementation of the install_mapping interface **
 *****************************************************/

class Platform_thread_registry : Noncopyable
{
	private:

		List<Platform_thread> _threads;
		Lock                  _lock;

	public:

		void insert(Platform_thread &thread)
		{
			Lock::Guard guard(_lock);
			_threads.insert(&thread);
		}

		void remove(Platform_thread &thread)
		{
			Lock::Guard guard(_lock);
			_threads.remove(&thread);
		}

		bool install_mapping(Mapping const &mapping, unsigned long pager_object_badge)
		{
			unsigned installed = 0;
			bool     result    = true;

			Lock::Guard guard(_lock);

			for (Platform_thread *t = _threads.first(); t; t = t->next()) {
				if (t->pager_object_badge() == pager_object_badge) {
					bool ok = t->install_mapping(mapping);
					if (!ok)
						result = false;
					installed ++;
				}
			}

			if (installed != 1) {
				Genode::error("install mapping is wrong ", installed,
				              " result=", result);
				result = false;
			}

			return result;
		}
};


Platform_thread_registry &platform_thread_registry()
{
	static Platform_thread_registry inst;
	return inst;
}


bool Genode::install_mapping(Mapping const &mapping, unsigned long pager_object_badge)
{
	return platform_thread_registry().install_mapping(mapping, pager_object_badge);
}


/********************************************************
 ** Utilities to support the Platform_thread interface **
 ********************************************************/

static void prepopulate_ipc_buffer(addr_t ipc_buffer_phys, Cap_sel ep_sel,
                                   Cap_sel lock_sel)
{
	/* IPC buffer is one page */
	size_t const page_rounded_size = get_page_size();

	/* allocate range in core's virtual address space */
	void *virt_addr;
	if (!platform()->region_alloc()->alloc(page_rounded_size, &virt_addr)) {
		error("could not allocate virtual address range in core of size ",
		      page_rounded_size);
		return;
	}

	/* map the IPC buffer to core-local virtual addresses */
	map_local(ipc_buffer_phys, (addr_t)virt_addr, 1);

	/* populate IPC buffer with thread information */
	Native_utcb &utcb = *(Native_utcb *)virt_addr;
	utcb.ep_sel = ep_sel.value();
	utcb.lock_sel = lock_sel.value();

	/* unmap IPC buffer from core */
	if (!unmap_local((addr_t)virt_addr, 1)) {
		Genode::error("could not unmap core virtual address ",
		              virt_addr, " in ", __PRETTY_FUNCTION__);
		return;
	}

	/* free core's virtual address space */
	platform()->region_alloc()->free(virt_addr, page_rounded_size);
}


/*******************************
 ** Platform_thread interface **
 *******************************/

int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	ASSERT(_pd);
	ASSERT(_pager);

	/* pager endpoint in core */
	Cap_sel const pager_sel(Capability_space::ipc_cap_data(_pager->cap()).sel);

	/* install page-fault handler endpoint selector to the PD's CSpace */
	_pd->cspace_cnode(_fault_handler_sel).copy(platform_specific()->core_cnode(),
	                                           pager_sel, _fault_handler_sel);

	/* install the thread's endpoint selector to the PD's CSpace */
	_pd->cspace_cnode(_ep_sel).copy(platform_specific()->core_cnode(),
	                                _info.ep_sel, _ep_sel);

	/* install the thread's notification object to the PD's CSpace */
	_pd->cspace_cnode(_lock_sel).mint(platform_specific()->core_cnode(),
	                                  _info.lock_sel, _lock_sel);

	/*
	 * Populate the thread's IPC buffer with initial information about the
	 * thread. Once started, the thread picks up this information in the
	 * 'Thread::_thread_bootstrap' method.
	 */
	prepopulate_ipc_buffer(_info.ipc_buffer_phys, _ep_sel, _lock_sel);

	/* bind thread to PD and CSpace */
	seL4_CapData_t const guard_cap_data =
		seL4_CapData_Guard_new(0, CONFIG_WORD_SIZE - _pd->cspace_size_log2());

	seL4_CapData_t const no_cap_data = { { 0 } };

	int const ret = seL4_TCB_SetSpace(_info.tcb_sel.value(), _fault_handler_sel.value(),
	                                  _pd->cspace_cnode_1st().sel().value(), guard_cap_data,
	                                  _pd->page_directory_sel().value(), no_cap_data);
	ASSERT(ret == 0);

	start_sel4_thread(_info.tcb_sel, (addr_t)ip, (addr_t)(sp), _location.xpos());
	return 0;
}


void Platform_thread::pause()
{
	int const ret = seL4_TCB_Suspend(_info.tcb_sel.value());
	if (ret != seL4_NoError)
		error("pausing thread failed with ", ret);
}


void Platform_thread::resume()
{
	int const ret = seL4_TCB_Resume(_info.tcb_sel.value());
	if (ret != seL4_NoError)
		error("pausing thread failed with ", ret);
}


void Platform_thread::state(Thread_state s)
{
	warning(__PRETTY_FUNCTION__, " not implemented");
	throw Cpu_thread::State_access_failed();
}


void Platform_thread::cancel_blocking()
{
	seL4_Signal(_info.lock_sel.value());
}


bool Platform_thread::install_mapping(Mapping const &mapping)
{
	return _pd->install_mapping(mapping, name());
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned priority,
                                 Affinity::Location location, addr_t utcb)
:
	_name(name),
	_utcb(utcb),
	_pager_obj_sel(platform_specific()->core_sel_alloc().alloc()),
	_location(location),
	_priority(Cpu_session::scale_priority(CONFIG_NUM_PRIORITIES, priority))

{
	static_assert(CONFIG_NUM_PRIORITIES == 256, " unknown priority configuration");

	if (_priority > 0)
		_priority -= 1;

	_info.init(_utcb ? _utcb : INITIAL_IPC_BUFFER_VIRT, _priority);
	platform_thread_registry().insert(*this);
}


Platform_thread::~Platform_thread()
{
	if (_pd) {
		seL4_TCB_Suspend(_info.tcb_sel.value());
		_pd->unbind_thread(this);
	}

	if (_pager) {
		Cap_sel const pager_sel(Capability_space::ipc_cap_data(_pager->cap()).sel);
		seL4_CNode_Revoke(seL4_CapInitThreadCNode, pager_sel.value(), 32);
	}

	seL4_CNode_Revoke(seL4_CapInitThreadCNode, _info.lock_sel.value(), 32);
	seL4_CNode_Revoke(seL4_CapInitThreadCNode, _info.ep_sel.value(), 32);

	_info.destruct();

	platform_thread_registry().remove(*this);
	platform_specific()->core_sel_alloc().free(_pager_obj_sel);
}

unsigned long long Platform_thread::execution_time() const
{
	Genode::Thread * me = Genode::Thread::myself();

	if (!me || !me->utcb()) {
		Genode::error("don't know myself");
		return 0;
	}

	seL4_IPCBuffer * ipcbuffer = reinterpret_cast<seL4_IPCBuffer *>(me->utcb());
	uint64_t const * values = reinterpret_cast<uint64_t *>(ipcbuffer->msg);

	/* kernel puts execution time on ipc buffer of calling thread */
	seL4_BenchmarkGetThreadUtilisation(_info.tcb_sel.value());

	uint64_t const execution_time = values[BENCHMARK_TCB_UTILISATION];
	return execution_time;
}
