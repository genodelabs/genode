/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		void install_mapping(Mapping const &mapping, unsigned long pager_object_badge)
		{
			Lock::Guard guard(_lock);

			for (Platform_thread *t = _threads.first(); t; t = t->next()) {
				if (t->pager_object_badge() == pager_object_badge)
					t->install_mapping(mapping);
			}
		}
};


Platform_thread_registry &platform_thread_registry()
{
	static Platform_thread_registry inst;
	return inst;
}


void Genode::install_mapping(Mapping const &mapping, unsigned long pager_object_badge)
{
	platform_thread_registry().install_mapping(mapping, pager_object_badge);
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
	unmap_local((addr_t)virt_addr, 1);

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
		seL4_CapData_Guard_new(0, 32 - _pd->cspace_size_log2());

	seL4_CapData_t const no_cap_data = { { 0 } };

	int const ret = seL4_TCB_SetSpace(_info.tcb_sel.value(), _fault_handler_sel.value(),
	                                  _pd->cspace_cnode_1st().sel().value(), guard_cap_data,
	                                  _pd->page_directory_sel().value(), no_cap_data);
	ASSERT(ret == 0);

	start_sel4_thread(_info.tcb_sel, (addr_t)ip, (addr_t)(sp));
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


Thread_state Platform_thread::state()
{
	seL4_TCB   const thread         = _info.tcb_sel.value();
	seL4_Bool  const suspend_source = false;
	seL4_Uint8 const arch_flags     = 0;
	seL4_UserContext registers;
	seL4_Word  const register_count = sizeof(registers) / sizeof(registers.eip);

	int const ret = seL4_TCB_ReadRegisters(thread, suspend_source, arch_flags,
	                                       register_count, &registers);
	if (ret != seL4_NoError) {
		error("reading thread state ", ret);
		throw Cpu_thread::State_access_failed();
	}

	Thread_state state;
	state.ip     = registers.eip;
	state.sp     = registers.esp;
	state.edi    = registers.edi;
	state.esi    = registers.esi;
	state.ebp    = registers.ebp;
	state.ebx    = registers.ebx;
	state.edx    = registers.edx;
	state.ecx    = registers.ecx;
	state.eax    = registers.eax;
	state.gs     = registers.gs;
	state.fs     = registers.fs;
	state.eflags = registers.eflags;
	state.trapno = 0; /* XXX detect/track if in exception and report here */
	/* registers.tls_base unused */

	return state;
}


void Platform_thread::cancel_blocking()
{
	seL4_Signal(_info.lock_sel.value());
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	ASSERT(_pd);
	return _pd->weak_ptr();
}


void Platform_thread::install_mapping(Mapping const &mapping)
{
	_pd->install_mapping(mapping);
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned priority,
                                 Affinity::Location, addr_t utcb)
:
	_name(name),
	_utcb(utcb),
	_pager_obj_sel(platform_specific()->core_sel_alloc().alloc())

{
	_info.init(_utcb ? _utcb : INITIAL_IPC_BUFFER_VIRT);
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


