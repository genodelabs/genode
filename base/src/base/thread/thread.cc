/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-11
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/env.h>
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


/******************************
 ** Thread-context allocator **
 ******************************/

Thread_base::Context *Thread_base::Context_allocator::base_to_context(addr_t base)
{
	addr_t result = base + Native_config::context_virtual_size()
	                - sizeof(Context);
	return reinterpret_cast<Context *>(result);
}


addr_t Thread_base::Context_allocator::addr_to_base(void *addr)
{
	return ((addr_t)addr) & ~(Native_config::context_virtual_size() - 1);
}


bool Thread_base::Context_allocator::_is_in_use(addr_t base)
{
	List_element<Thread_base> *le = _threads.first();
	for (; le; le = le->next())
		if (base_to_context(base) == le->object()->_context)
			return true;

	return false;
}


Thread_base::Context *Thread_base::Context_allocator::alloc(Thread_base *thread_base)
{
	Lock::Guard _lock_guard(_threads_lock);

	/*
	 * Find slot in context area for the new context
	 */
	addr_t base = Native_config::context_area_virtual_base();
	for (; _is_in_use(base); base += Native_config::context_virtual_size()) {

		/* check upper bound of context area */
		if (base >= Native_config::context_area_virtual_base() +
		    Native_config::context_area_virtual_size())
			return 0;
	}

	_threads.insert(&thread_base->_list_element);

	return base_to_context(base);
}


void Thread_base::Context_allocator::free(Thread_base *thread_base)
{
	Lock::Guard _lock_guard(_threads_lock);

	_threads.remove(&thread_base->_list_element);
}


/*****************
 ** Thread base **
 *****************/

Thread_base::Context_allocator *Thread_base::_context_allocator()
{
	static Context_allocator context_allocator_inst;
	return &context_allocator_inst;
}


Thread_base::Context *Thread_base::_alloc_context(size_t stack_size)
{
	/*
	 * Synchronize context list when creating new threads from multiple threads
	 *
	 * XXX: remove interim fix
	 */
	static Lock alloc_lock;
	Lock::Guard _lock_guard(alloc_lock);

	/* allocate thread context */
	Context *context = _context_allocator()->alloc(this);
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
		env_context_area_rm_session()->attach_at(ds_cap, attach_addr, ds_size);

	} catch (Ram_session::Alloc_failed) {
		throw Stack_alloc_failed();
	}

	/*
	 * Now the thread context is backed by memory, so it is safe to access its
	 * members.
	 */

	context->thread_base = this;
	context->stack_base  = ds_addr;
	context->ds_cap      = ds_cap;
	return context;
}


void Thread_base::_free_context()
{
	addr_t ds_addr = _context->stack_base -
	                 Native_config::context_area_virtual_base();
	Ram_dataspace_capability ds_cap = _context->ds_cap;
	Genode::env_context_area_rm_session()->detach((void *)ds_addr);
	Genode::env_context_area_ram_session()->free(ds_cap);
	_context_allocator()->free(this);
}


void Thread_base::name(char *dst, size_t dst_len)
{
	snprintf(dst, min(dst_len, (size_t)Context::NAME_LEN), "%s", _context->name);
}


Thread_base *Thread_base::myself()
{
	int dummy = 0; /* used for determining the stack pointer */

	/*
	 * If the stack pointer is outside the thread-context area, we assume that
	 * we are the main thread because this condition can never met by any other
	 * thread.
	 */
	addr_t sp = (addr_t)(&dummy);
	if (sp <  Native_config::context_area_virtual_base() ||
	    sp >= Native_config::context_area_virtual_base() +
	          Native_config::context_area_virtual_size())
		return 0;

	addr_t base = Context_allocator::addr_to_base(&dummy);
	return Context_allocator::base_to_context(base)->thread_base;
}


Thread_base::Thread_base(const char *name, size_t stack_size)
:
	_list_element(this),
	_context(_alloc_context(stack_size))
{
	strncpy(_context->name, name, sizeof(_context->name));
	_init_platform_thread();
}


Thread_base::~Thread_base()
{
	_deinit_platform_thread();
	_free_context();
}
