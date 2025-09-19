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

	return _mappings.alloc([&] (Mappings::Entry &mapping) {

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

						mapping.ds_cap = allocation.cap;
						mapping.base   = ds_addr + stack_area_virtual_base();

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
	},
	[&] /* too many mappings */ () -> Size_result {
		return Error::STACK_AREA_EXHAUSTED; });
}


Thread::Alloc_stack_result
Thread::_alloc_stack(Runtime &runtime, Name const &name, Stack_size size)
{
	Stack *stack_ptr = Stack_allocator::stack_allocator().alloc(this);
	if (!stack_ptr)
		return Stack_error::STACK_AREA_EXHAUSTED;

	return _alloc_stack(runtime, *stack_ptr, name, size);
}


Thread::Alloc_stack_result Thread::_alloc_main_stack(Runtime &runtime)
{
	Stack *stack_ptr = Stack_allocator::stack_allocator().alloc_main(this);
	if (!stack_ptr)
		return Stack_error::STACK_AREA_EXHAUSTED;

	return _alloc_stack(runtime, *stack_ptr, "main", Stack_size { 16*1024 });
}


Thread::Alloc_stack_result
Thread::_alloc_stack(Runtime &, Stack &stack, Name const &name, Stack_size stack_size)
{
	/* determine size of dataspace to allocate for the stack */
	enum { PAGE_SIZE_LOG2 = 12 };
	size_t ds_size = align_addr(stack_size.num_bytes, PAGE_SIZE_LOG2);

	if (stack_size.num_bytes >= stack_virtual_size() -
	    sizeof(Native_utcb) - (1UL << PAGE_SIZE_LOG2))
		return Stack_error::STACK_TOO_LARGE;

	/*
	 * Calculate base address of the stack
	 *
	 * The stack pointer is always located at the top of the stack header.
	 */
	addr_t ds_addr = Stack_allocator::addr_to_base(&stack) +
	                 stack_virtual_size() - ds_size;

	/* add padding for UTCB if defined for the platform */
	if (sizeof(Native_utcb) >= (1 << PAGE_SIZE_LOG2))
		ds_addr -= sizeof(Native_utcb);

	Ram_allocator &ram = *env_stack_area_ram_allocator;

	/* allocate and attach backing store for the stack */
	return ram.try_alloc(ds_size).convert<Alloc_stack_result>([&] (Ram::Allocation &allocation)
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
					construct_at<Stack>(&stack, name, *this,
					                    Stack::Mappings::Entry { ds_addr, allocation.cap });

					Abi::init_stack(stack.top());
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
	Stack::Mappings mappings = stack.mappings();

	/* call de-constructor explicitly before memory gets detached */
	stack.~Stack();

	mappings.for_each([&] (addr_t at, Ram_dataspace_capability ds_cap) {
		Genode::env_stack_area_region_map->detach(at - stack_area_virtual_base());

		/* deallocate RAM block */
		{
			Ram::Allocation { *env_stack_area_ram_allocator, { ds_cap, 0 } };
		}
	});

	/* stack ready for reuse */
	Stack_allocator::stack_allocator().free(stack);
}


static Thread::Stack_info stack_info(Stack const &stack)
{
	return { stack.base(), stack.top(),
	         stack_virtual_size() - stack.libc_tls_pointer_offset() };
}


Thread::Info_result Thread::info() const
{
	return _stack.convert<Info_result>(
		[&] (Stack const &stack) { return stack_info(stack); },
		[&] (Stack_error e)      { return e; });
}


void Thread::join() { _join.block(); }


Thread::Alloc_secondary_stack_result
Thread::alloc_secondary_stack(Name const &name, Stack_size size)
{
	Stack *stack_ptr = Stack_allocator::stack_allocator().alloc(this);
	if (!stack_ptr)
		return Stack_error::STACK_AREA_EXHAUSTED;

	return _alloc_stack(_runtime, *stack_ptr, name, size).convert<Alloc_secondary_stack_result>(
		[&] (Stack const &stack) { return (void *)stack.top(); },
		[&] (Stack_error e)      { return e; });
}


void Thread::free_secondary_stack(void* stack_addr)
{
	addr_t base = Stack_allocator::addr_to_base(stack_addr);
	_free_stack(*Stack_allocator::base_to_stack(base));
}


Thread::Stack_size_result Thread::stack_size(size_t const size)
{
	return _stack.convert<Stack_size_result>(
		[&] (Stack &stack)  { return stack.size(size); },
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


Thread::Thread(Env &env, Name const &name, Stack_size stack_size, Location location)
:
	Thread(env.runtime(), name, stack_size, location)
{ }


Thread::Thread(Runtime &runtime, Name const &name, Stack_size stack_size,
               Affinity::Location affinity)
:
	name(name), _runtime(runtime), _affinity(affinity),
	_stack(_alloc_stack(runtime, name, stack_size))
{
	_stack.with_result(
		[&] (Stack &stack) {
			_native_thread_ptr = &stack.native_thread();
			_init_native_thread(stack);
		},
		[&] (Stack_error) { /* error reflected by 'info()' */ });
}


Thread::Thread(Runtime &runtime)
:
	name("main"), _runtime(runtime), _affinity({ }),
	_stack(_alloc_main_stack(runtime))
{
	_stack.with_result(
		[&] (Stack &stack) {
			_native_thread_ptr = &stack.native_thread();
			_init_native_main_thread(stack);
		},
		[&] (Stack_error) { /* error reflected by 'info()' */ });
}


Thread::~Thread()
{
	if (Thread::myself() == this) {
		error("thread '", name, "' tried to self de-struct - sleeping forever.");
		sleep_forever();
	}

	_stack.with_result(
		[&] (Stack &stack) {
			_deinit_native_thread(stack);
			_free_stack(stack);
		},
		[&] (Stack_error) { });

	cxx_free_tls(this);
}
