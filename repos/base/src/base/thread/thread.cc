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

/* Genode includes */
#include <util/construct_at.h>
#include <util/string.h>
#include <util/misc_math.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/snprintf.h>

/* base-internal includes */
#include <base/internal/stack_allocator.h>

using namespace Genode;


/**
 * Return the managed dataspace holding the stack area
 *
 * This function is provided by the process environment.
 */
namespace Genode {

	Rm_session  *env_stack_area_rm_session();
	Ram_session *env_stack_area_ram_session();
}


void Stack::size(size_t const size)
{
	/* check if the stack needs to be enhanced */
	size_t const stack_size = (addr_t)_stack - _base;
	if (stack_size >= size) { return; }

	/* check if the stack enhancement fits the stack region */
	enum {
		UTCB_SIZE         = sizeof(Native_utcb),
		PAGE_SIZE_LOG2    = 12,
		PAGE_SIZE         = (1UL << PAGE_SIZE_LOG2),
	};
	addr_t const stack_slot_base = Stack_allocator::addr_to_base(this);
	size_t const ds_size = align_addr(size - stack_size, PAGE_SIZE_LOG2);
	if (_base - ds_size < stack_slot_base)
		throw Thread_base::Stack_too_large();

	/* allocate and attach backing store for the stack enhancement */
	addr_t const ds_addr = _base - ds_size -
	                       Native_config::stack_area_virtual_base();
	try {
		Ram_session * const ram = env_stack_area_ram_session();
		Ram_dataspace_capability const ds_cap = ram->alloc(ds_size);
		Rm_session * const rm = env_stack_area_rm_session();
		void * const attach_addr = rm->attach_at(ds_cap, ds_addr, ds_size);

		if (ds_addr != (addr_t)attach_addr)
			throw Thread_base::Out_of_stack_space();
	}
	catch (Ram_session::Alloc_failed) {
		throw Thread_base::Stack_alloc_failed();
	}

	/* update stack information */
	_base -= ds_size;
}


Stack *
Thread_base::_alloc_stack(size_t stack_size, char const *name, bool main_thread)
{
	/*
	 * Synchronize stack list when creating new threads from multiple threads
	 *
	 * XXX: remove interim fix
	 */
	static Lock alloc_lock;
	Lock::Guard _lock_guard(alloc_lock);

	/* allocate stack */
	Stack *stack = Stack_allocator::stack_allocator().alloc(this, main_thread);
	if (!stack)
		throw Out_of_stack_space();

	/* determine size of dataspace to allocate for the stack */
	enum { PAGE_SIZE_LOG2 = 12 };
	size_t ds_size = align_addr(stack_size, PAGE_SIZE_LOG2);

	if (stack_size >= Native_config::stack_virtual_size() -
	    sizeof(Native_utcb) - (1UL << PAGE_SIZE_LOG2))
		throw Stack_too_large();

	/*
	 * Calculate base address of the stack
	 *
	 * The stack pointer is always located at the top of the stack header.
	 */
	addr_t ds_addr = Stack_allocator::addr_to_base(stack) +
	                 Native_config::stack_virtual_size() -
	                 ds_size;

	/* add padding for UTCB if defined for the platform */
	if (sizeof(Native_utcb) >= (1 << PAGE_SIZE_LOG2))
		ds_addr -= sizeof(Native_utcb);

	/* allocate and attach backing store for the stack */
	Ram_dataspace_capability ds_cap;
	try {
		ds_cap = env_stack_area_ram_session()->alloc(ds_size);
		addr_t attach_addr = ds_addr - Native_config::stack_area_virtual_base();
		if (attach_addr != (addr_t)env_stack_area_rm_session()->attach_at(ds_cap, attach_addr, ds_size))
			throw Stack_alloc_failed();
	}
	catch (Ram_session::Alloc_failed) { throw Stack_alloc_failed(); }

	/*
	 * Now the stack is backed by memory, so it is safe to access its members.
	 *
	 * We need to initialize the stack object's memory with zeroes, otherwise
	 * the ds_cap isn't invalid. That would cause trouble when the assignment
	 * operator of Native_capability is used.
	 */
	construct_at<Stack>(stack, name, *this, ds_addr, ds_cap);

	Abi::init_stack(stack->top());
	return stack;
}


void Thread_base::_free_stack(Stack *stack)
{
	addr_t ds_addr = stack->base() - Native_config::stack_area_virtual_base();
	Ram_dataspace_capability ds_cap = stack->ds_cap();

	/* call de-constructor explicitly before memory gets detached */
	stack->~Stack();

	Genode::env_stack_area_rm_session()->detach((void *)ds_addr);
	Genode::env_stack_area_ram_session()->free(ds_cap);

	/* stack ready for reuse */
	Stack_allocator::stack_allocator().free(stack);
}


void Thread_base::name(char *dst, size_t dst_len)
{
	snprintf(dst, dst_len, "%s", _stack->name().string());
}


void Thread_base::join() { _join_lock.lock(); }


void* Thread_base::alloc_secondary_stack(char const *name, size_t stack_size)
{
	Stack *stack = _alloc_stack(stack_size, name, false);
	return (void *)stack->top();
}


void Thread_base::free_secondary_stack(void* stack_addr)
{
	addr_t base = Stack_allocator::addr_to_base(stack_addr);
	_free_stack(Stack_allocator::base_to_stack(base));
}


void *Thread_base::stack_top() const { return (void *)_stack->top(); }


void *Thread_base::stack_base() const { return (void*)_stack->base(); }


void Thread_base::stack_size(size_t const size) { _stack->size(size); }


Thread_base::Thread_base(size_t weight, const char *name, size_t stack_size,
                         Type type, Cpu_session *cpu_session)
:
	_cpu_session(cpu_session),
	_trace_control(nullptr),
	_stack(type == REINITIALIZED_MAIN ?
	         _stack : _alloc_stack(stack_size, name, type == MAIN)),
	_join_lock(Lock::LOCKED)
{
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
		     _stack->name().string());
		sleep_forever();
	}

	_deinit_platform_thread();
	_free_stack(_stack);

	/*
	 * We have to detach the trace control dataspace last because
	 * we cannot invalidate the pointer used by the Trace::Logger
	 * from here and any following RPC call will stumple upon the
	 * detached trace control dataspace.
	 */
	if (_trace_control)
		env()->rm_session()->detach(_trace_control);
}
