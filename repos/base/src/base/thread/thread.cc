/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-11
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/snprintf.h>
#include <util/string.h>
#include <util/misc_math.h>

using namespace Genode;


/**
 * Return the managed dataspace holding the thread context area
 *
 * This function is provided by the process environment.
 */
namespace Genode {

	Rm_session  *env_context_area_rm_session();
	Ram_session *env_context_area_ram_session();
}


void Thread_base::Context::stack_size(size_t const size)
{
	/* check if the stack needs to be enhanced */
	size_t const stack_size = (addr_t)_stack - stack_base;
	if (stack_size >= size) { return; }

	/* check if the stack enhancement fits the context region */
	enum {
		CONTEXT_SIZE      = Native_config::context_virtual_size(),
		CONTEXT_AREA_BASE = Native_config::context_area_virtual_base(),
		UTCB_SIZE         = sizeof(Native_utcb),
		PAGE_SIZE_LOG2    = 12,
		PAGE_SIZE         = (1UL << PAGE_SIZE_LOG2),
	};
	addr_t const context_base = Context_allocator::addr_to_base(this);
	size_t const ds_size = align_addr(size - stack_size, PAGE_SIZE_LOG2);
	if (stack_base - ds_size < context_base) { throw Stack_too_large(); }

	/* allocate and attach backing store for the stack enhancement */
	addr_t const ds_addr = stack_base - ds_size - CONTEXT_AREA_BASE;
	try {
		Ram_session * const ram = env_context_area_ram_session();
		Ram_dataspace_capability const ds_cap = ram->alloc(ds_size);
		Rm_session * const rm = env_context_area_rm_session();
		void * const attach_addr = rm->attach_at(ds_cap, ds_addr, ds_size);
		if (ds_addr != (addr_t)attach_addr) { throw Stack_alloc_failed(); }
	}
	catch (Ram_session::Alloc_failed) { throw Stack_alloc_failed(); }

	/* update context information */
	stack_base -= ds_size;
}


Thread_base::Context *
Thread_base::_alloc_context(size_t stack_size, bool main_thread)
{
	/*
	 * Synchronize context list when creating new threads from multiple threads
	 *
	 * XXX: remove interim fix
	 */
	static Lock alloc_lock;
	Lock::Guard _lock_guard(alloc_lock);

	/* allocate thread context */
	Context *context = _context_allocator()->alloc(this, main_thread);
	if (!context)
		throw Context_alloc_failed();

	/* determine size of dataspace to allocate for context members and stack */
	enum { PAGE_SIZE_LOG2 = 12 };
	size_t ds_size = align_addr(stack_size, PAGE_SIZE_LOG2);

	if (stack_size >= Native_config::context_virtual_size() -
	    sizeof(Native_utcb) - (1UL << PAGE_SIZE_LOG2))
		throw Stack_too_large();

	/*
	 * Calculate base address of the stack
	 *
	 * The stack is always located at the top of the context.
	 */
	addr_t ds_addr = Context_allocator::addr_to_base(context) +
	                 Native_config::context_virtual_size() -
	                 ds_size;

	/* add padding for UTCB if defined for the platform */
	if (sizeof(Native_utcb) >= (1 << PAGE_SIZE_LOG2))
		ds_addr -= sizeof(Native_utcb);

	/* allocate and attach backing store for the stack */
	Ram_dataspace_capability ds_cap;
	try {
		ds_cap = env_context_area_ram_session()->alloc(ds_size);
		addr_t attach_addr = ds_addr - Native_config::context_area_virtual_base();
		if (attach_addr != (addr_t)env_context_area_rm_session()->attach_at(ds_cap, attach_addr, ds_size))
			throw Stack_alloc_failed();
	}
	catch (Ram_session::Alloc_failed) { throw Stack_alloc_failed(); }

	/*
	 * Now the thread context is backed by memory, so it is safe to access its
	 * members.
	 *
	 * We need to initialize the context object's memory with zeroes,
	 * otherwise the ds_cap isn't invalid. That would cause trouble
	 * when the assignment operator of Native_capability is used.
	 */
	memset(context, 0, sizeof(Context) - sizeof(Context::utcb));
	context->thread_base = this;
	context->stack_base  = ds_addr;
	context->ds_cap      = ds_cap;

	Abi::init_stack(context->stack_top());
	return context;
}


void Thread_base::_free_context(Context* context)
{
	addr_t ds_addr = context->stack_base - Native_config::context_area_virtual_base();
	Ram_dataspace_capability ds_cap = context->ds_cap;

	/* call de-constructor explicitly before memory gets detached */
	context->~Context();

	Genode::env_context_area_rm_session()->detach((void *)ds_addr);
	Genode::env_context_area_ram_session()->free(ds_cap);

	/* context area ready for reuse */
	_context_allocator()->free(context);
}


void Thread_base::name(char *dst, size_t dst_len)
{
	snprintf(dst, min(dst_len, (size_t)Context::NAME_LEN), "%s", _context->name);
}


void Thread_base::join() { _join_lock.lock(); }


void* Thread_base::alloc_secondary_stack(char const *name, size_t stack_size)
{
	Context *context = _alloc_context(stack_size, false);
	strncpy(context->name, name, sizeof(context->name));
	return (void *)context->stack_top();
}


void Thread_base::free_secondary_stack(void* stack_addr)
{
	addr_t base = Context_allocator::addr_to_base(stack_addr);
	_free_context(Context_allocator::base_to_context(base));
}


Thread_base::Thread_base(size_t weight, const char *name, size_t stack_size,
                         Type type, Cpu_session *cpu_session)
:
	_cpu_session(cpu_session),
	_trace_control(nullptr),
	_context(type == REINITIALIZED_MAIN ?
	         _context : _alloc_context(stack_size, type == MAIN)),
	_join_lock(Lock::LOCKED)
{
	strncpy(_context->name, name, sizeof(_context->name));
	_init_platform_thread(weight, type);

	if (_cpu_session) {
		Dataspace_capability ds = _cpu_session->trace_control();
		if (ds.valid())
			_trace_control = env()->rm_session()->attach(ds);
	}
}


Thread_base::Thread_base(size_t weight, const char *name, size_t stack_size,
                         Type type)
: Thread_base(weight, name, stack_size, type, nullptr) { }


Thread_base::~Thread_base()
{
	if (Thread_base::myself() == this) {
		PERR("thread '%s' tried to self de-struct - sleeping forever.",
		     _context->name);
		sleep_forever();
	}

	_deinit_platform_thread();
	_free_context(_context);

	/*
	 * We have to detach the trace control dataspace last because
	 * we cannot invalidate the pointer used by the Trace::Logger
	 * from here and any following RPC call will stumple upon the
	 * detached trace control dataspace.
	 */
	env()->rm_session()->detach(_trace_control);
}
