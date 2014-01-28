/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-11
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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


size_t Thread_base::Context_allocator::base_to_idx(addr_t base)
{
	/* the first context isn't managed through the indices */
	return ((base - Native_config::context_area_virtual_base()) /
	       Native_config::context_virtual_size()) - 1;
}


addr_t Thread_base::Context_allocator::idx_to_base(size_t idx)
{
	/* the first context isn't managed through the indices */
	return Native_config::context_area_virtual_base() +
	       (idx + 1) * Native_config::context_virtual_size();
}


Thread_base::Context *
Thread_base::Context_allocator::alloc(Thread_base *thread_base, bool main_thread)
{
	Lock::Guard _lock_guard(_threads_lock);
	try {
		addr_t base;
		if (main_thread) {
			/* the main-thread context isn't managed by '_alloc' */
			base = Native_config::context_area_virtual_base();
		} else {
			/* contexts besides main-thread context are managed by '_alloc' */
			base = idx_to_base(_alloc.alloc());
		}
		return base_to_context(base);
	} catch(Bit_allocator<MAX_THREADS>::Out_of_indices) {
		return 0;
	}
}


void Thread_base::Context_allocator::free(Context *context)
{
	Lock::Guard _lock_guard(_threads_lock);
	addr_t const base = addr_to_base(context);

	/* the main-thread context isn't managed by '_alloc' */
	if (base == Native_config::context_area_virtual_base()) { return; }

	/* contexts besides main-thread context are managed by '_alloc' */
	_alloc.free(base_to_idx(base));
}


/*****************
 ** Thread base **
 *****************/

Thread_base::Context_allocator *Thread_base::_context_allocator()
{
	static Context_allocator context_allocator_inst;
	return &context_allocator_inst;
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

	/*
	 * The value at the top of the stack might get interpreted as return
	 * address of the thread start function by GDB, so we set it to 0.
	 */
	*(addr_t*)context->stack_top() = 0;

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


void Thread_base::join()
{
	_join_lock.lock();
}


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


Thread_base::Thread_base(const char *name, size_t stack_size, Type type)
:
	_context(type == REINITIALIZED_MAIN ?
	                 _context : _alloc_context(stack_size, type == MAIN)),
	_join_lock(Lock::LOCKED)
{
	strncpy(_context->name, name, sizeof(_context->name));
	_init_platform_thread(type);
}


Thread_base::~Thread_base()
{
	_deinit_platform_thread();
	_free_context(_context);
}
