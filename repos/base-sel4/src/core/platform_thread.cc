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
#include <base/printf.h>
#include <util/string.h>

/* core includes */
#include <platform_thread.h>
#include <platform_pd.h>

/* base-internal includes */
#include <internal/capability_space_sel4.h>

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


/*******************************
 ** Platform_thread interface **
 *******************************/

int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	ASSERT(_pd);
	ASSERT(_pager);

	/* allocate fault handler selector in the PD's CSpace */
	_fault_handler_sel = _pd->alloc_sel();

	/* pager endpoint in core */
	unsigned const pager_sel = Capability_space::ipc_cap_data(_pager->cap()).sel;

	/* install page-fault handler endpoint selector to the PD's CSpace */
	_pd->cspace_cnode().copy(platform_specific()->core_cnode(), pager_sel,
	                         _fault_handler_sel);

	_pd->map_ipc_buffer_of_initial_thread(_info.ipc_buffer_phys);

	/* bind thread to PD and CSpace */
	seL4_CapData_t const guard_cap_data =
		seL4_CapData_Guard_new(0, 32 - _pd->cspace_size_log2());

	seL4_CapData_t const no_cap_data = { { 0 } };

	int const ret = seL4_TCB_SetSpace(_info.tcb_sel, _fault_handler_sel,
	                                  _pd->cspace_cnode().sel(), guard_cap_data,
	                                  _pd->page_directory_sel(), no_cap_data);
	ASSERT(ret == 0);

	start_sel4_thread(_info.tcb_sel, (addr_t)ip, (addr_t)(sp));
	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}


void Platform_thread::state(Thread_state s)
{
	PDBG("not implemented");
	throw Cpu_session::State_access_failed();
}


Thread_state Platform_thread::state()
{
	PDBG("not implemented");
	throw Cpu_session::State_access_failed();
}


void Platform_thread::cancel_blocking()
{
	PDBG("not implemented");
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
                                 addr_t utcb)
:
	_name(name),
	_pager_obj_sel(platform_specific()->alloc_core_sel())

{
	_info.init(Platform_pd::INITIAL_IPC_BUFFER_VIRT);
	platform_thread_registry().insert(*this);
}


Platform_thread::~Platform_thread()
{
	PDBG("not completely implemented");

	platform_thread_registry().remove(*this);
	platform_specific()->free_core_sel(_pager_obj_sel);
}


