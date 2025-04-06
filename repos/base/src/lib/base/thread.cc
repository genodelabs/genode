/*
 * \brief  Implementation of the Thread API
 * \author Norman Feske
 * \date   2010-01-11
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <util/string.h>
#include <util/misc_math.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>

/* base-internal includes */
#include <base/internal/stack_allocator.h>
#include <base/internal/globals.h>

using namespace Genode;
using Local_rm = Local::Constrained_region_map;


static Local_rm    *local_rm_ptr;
static Cpu_session *cpu_session_ptr;


Stack::Size_result Stack::size(size_t const size)
{
	/* check if the stack needs to be enhanced */
	size_t const stack_size = (addr_t)_stack - _base;
	if (stack_size >= size)
		return stack_size;

	/* check if the stack enhancement fits the stack region */
	enum {
		UTCB_SIZE      = sizeof(Native_utcb),
		PAGE_SIZE_LOG2 = 12,
		PAGE_SIZE      = (1UL << PAGE_SIZE_LOG2),
	};
	addr_t const stack_slot_base = Stack_allocator::addr_to_base(this);
	size_t const ds_size = align_addr(size - stack_size, PAGE_SIZE_LOG2);
	if (_base - ds_size < stack_slot_base)
		return Error::STACK_TOO_LARGE;

	/* allocate and attach backing store for the stack enhancement */
	addr_t const ds_addr = _base - ds_size - stack_area_virtual_base();

	Ram_allocator &ram = *env_stack_area_ram_allocator;
	Region_map    &rm  = *env_stack_area_region_map;

	return ram.try_alloc(ds_size).convert<Size_result>(
		[&] (Ram::Allocation &allocation) {

			return rm.attach(allocation.cap, Region_map::Attr {
				.size       = ds_size,
				.offset     = 0,
				.use_at     = true,
				.at         = ds_addr,
				.executable = { },
				.writeable  = true,
			}).convert<Size_result> (

				[&] (Region_map::Range r) -> Size_result {
					if (r.start != ds_addr)
						return Error::STACK_AREA_EXHAUSTED;

					/* update stack information */
					_base -= ds_size;

					allocation.deallocate = false;

					return (addr_t)_stack - _base;
				},
				[&] (Region_map::Attach_error) {
					return Error::STACK_AREA_EXHAUSTED; }
			);
		},
		[&] (Ram_allocator::Alloc_error) {
			return Error::STACK_AREA_EXHAUSTED; }
	);
}


Thread::Alloc_stack_result
Thread::_alloc_stack(size_t stack_size, Name const &name, bool main_thread)
{
	/* allocate stack */
	Stack *stack = Stack_allocator::stack_allocator().alloc(this, main_thread);
	if (!stack)
		return Stack_error::STACK_AREA_EXHAUSTED;

	/* determine size of dataspace to allocate for the stack */
	enum { PAGE_SIZE_LOG2 = 12 };
	size_t ds_size = align_addr(stack_size, PAGE_SIZE_LOG2);

	if (stack_size >= stack_virtual_size() -
	    sizeof(Native_utcb) - (1UL << PAGE_SIZE_LOG2))
		return Stack_error::STACK_TOO_LARGE;

	/*
	 * Calculate base address of the stack
	 *
	 * The stack pointer is always located at the top of the stack header.
	 */
	addr_t ds_addr = Stack_allocator::addr_to_base(stack) +
	                 stack_virtual_size() - ds_size;

	/* add padding for UTCB if defined for the platform */
	if (sizeof(Native_utcb) >= (1 << PAGE_SIZE_LOG2))
		ds_addr -= sizeof(Native_utcb);

	Ram_allocator &ram = *env_stack_area_ram_allocator;

	/* allocate and attach backing store for the stack */
	return ram.try_alloc(ds_size).convert<Alloc_stack_result>(

	 [&] (Ram::Allocation &allocation)
	 {
			addr_t const attach_addr = ds_addr - stack_area_virtual_base();

			return env_stack_area_region_map->attach(allocation.cap, Region_map::Attr {
				.size       = ds_size,
				.offset     = { },
				.use_at     = true,
				.at         = attach_addr,
				.executable = { },
				.writeable  = true
			}).convert<Alloc_stack_result>(

				[&] (Region_map::Range const range) -> Alloc_stack_result {
					if (range.start != attach_addr)
						return Stack_error::STACK_TOO_LARGE;

					/*
					 * Now the stack is backed by memory, it is safe to access
					 * its members.
					 *
					 * We need to initialize the stack object's memory with
					 * zeroes, otherwise the ds_cap isn't invalid. That would
					 * cause trouble when the assignment operator of
					 * Native_capability is used.
					 */
					construct_at<Stack>(stack, name, *this, ds_addr, allocation.cap);

					Abi::init_stack(stack->top());
					allocation.deallocate = false;
					return stack;
				},
				[&] (Region_map::Attach_error) -> Alloc_stack_result {
					return Stack_error::STACK_AREA_EXHAUSTED; }
			);
		},
		[&] (Ram_allocator::Alloc_error) -> Alloc_stack_result {
			return Stack_error::STACK_AREA_EXHAUSTED; }
	);
}


void Thread::_free_stack(Stack &stack)
{
	addr_t ds_addr = stack.base() - stack_area_virtual_base();
	Ram_dataspace_capability ds_cap = stack.ds_cap();

	/* call de-constructor explicitly before memory gets detached */
	stack.~Stack();

	Genode::env_stack_area_region_map->detach(ds_addr);

	/* deallocate RAM block */
	{
		Ram::Allocation { *env_stack_area_ram_allocator, { ds_cap, 0 } };
	}

	/* stack ready for reuse */
	Stack_allocator::stack_allocator().free(stack);
}


static Thread::Stack_info stack_info(Stack &stack)
{
	return { stack.base(), stack.top(),
	         stack_virtual_size() - stack.libc_tls_pointer_offset() };
}


Thread::Info_result Thread::info() const
{
	return _stack.convert<Info_result>(
		[&] (Stack *stack)  { return stack_info(*stack); },
		[&] (Stack_error e) { return e; });
}


void Thread::join() { _join.block(); }


Thread::Alloc_secondary_stack_result
Thread::alloc_secondary_stack(Name const &name, size_t stack_size)
{
	return _alloc_stack(stack_size, name, false).convert<Alloc_secondary_stack_result>(
		[&] (Stack *stack)  { return (void *)stack->top(); },
		[&] (Stack_error e) { return e; });
}


void Thread::free_secondary_stack(void* stack_addr)
{
	addr_t base = Stack_allocator::addr_to_base(stack_addr);
	_free_stack(*Stack_allocator::base_to_stack(base));
}


Thread::Stack_size_result Thread::stack_size(size_t const size)
{
	return _stack.convert<Stack_size_result>(
		[&] (Stack *stack)  { return stack->size(size); },
		[&] (Stack_error e) { return e; });
}


Thread::Stack_info Thread::mystack()
{
	addr_t base = Stack_allocator::addr_to_base(&base);
	return stack_info(*Stack_allocator::base_to_stack(base));
}


size_t Thread::stack_virtual_size()
{
	return Genode::stack_virtual_size();
}


addr_t Thread::stack_area_virtual_base()
{
	return Genode::stack_area_virtual_base();
}


size_t Thread::stack_area_virtual_size()
{
	return Genode::stack_area_virtual_size();
}


Thread::Thread(size_t weight, const char *name, size_t stack_size,
               Type type, Cpu_session *cpu_session, Affinity::Location affinity)
:
	name(name),
	_cpu_session(cpu_session),
	_affinity(affinity),
	_trace_control(nullptr),
	_stack(_alloc_stack(stack_size, name, type == MAIN))
{
	_stack.with_result(
		[&] (Stack *stack) {
			_native_thread_ptr = &stack->native_thread();
			_init_native_thread(*stack, weight, type);
		},
		[&] (Stack_error) { /* error reflected by 'info()' */ });
}


void Thread::_init_cpu_session_and_trace_control()
{
	if (!cpu_session_ptr || !local_rm_ptr) {
		error("missing call of init_thread");
		return;
	}

	/* if no CPU session is given, use it from the environment */
	if (!_cpu_session) {
		_cpu_session = cpu_session_ptr; }

	/* initialize trace control now that the CPU session must be valid */
	Dataspace_capability ds = _cpu_session->trace_control();
	if (ds.valid()) {
		Region_map::Attr attr { };
		attr.writeable = true;
		local_rm_ptr->attach(ds, attr).with_result(
			[&] (Local_rm::Attachment &a) {
				a.deallocate = false;
				_trace_control = reinterpret_cast<Trace::Control *>(a.ptr); },
			[&] (Region_map::Attach_error) {
				error("failed to initialize trace control for new thread"); }
		);
	}
}


Thread::Thread(size_t weight, const char *name, size_t stack_size,
               Type type, Affinity::Location affinity)
:
	Thread(weight, name, stack_size, type, cpu_session_ptr, affinity)
{ }


Thread::Thread(Env &, Name const &name, size_t stack_size, Location location,
               Weight weight, Cpu_session &cpu)
:
	Thread(weight.value, name.string(), stack_size, NORMAL, &cpu, location)
{ }


Thread::Thread(Env &env, Name const &name, size_t stack_size)
:
	Thread(env, name, stack_size, Location(), Weight(), env.cpu())
{ }


Thread::~Thread()
{
	if (Thread::myself() == this) {
		error("thread '", name, "' tried to self de-struct - sleeping forever.");
		sleep_forever();
	}

	_stack.with_result(
		[&] (Stack *stack) {
			_deinit_native_thread(*stack);
			_free_stack(*stack);
		},
		[&] (Stack_error) { });

	cxx_free_tls(this);

	/*
	 * We have to detach the trace control dataspace last because
	 * we cannot invalidate the pointer used by the Trace::Logger
	 * from here and any following RPC call will stumple upon the
	 * detached trace control dataspace.
	 */
	if (_trace_control && local_rm_ptr)
		local_rm_ptr->detach(addr_t(_trace_control));
}


void Genode::init_thread(Cpu_session &cpu_session, Local_rm &local_rm)
{
	local_rm_ptr    = &local_rm;
	cpu_session_ptr = &cpu_session;
}
