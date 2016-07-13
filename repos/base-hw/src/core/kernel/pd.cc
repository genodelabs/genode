/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>
#include <base/log.h>
#include <util.h>

/* Genode includes */
#include <assert.h>
#include <page_flags.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Kernel;

/* structure of the mode transition */
extern int            _mt_begin;
extern int            _mt_end;
extern int            _mt_user_entry_pic;
extern Genode::addr_t _mt_client_context_ptr;
extern Genode::addr_t _mt_master_context_begin;
extern Genode::addr_t _mt_master_context_end;


size_t Mode_transition_control::_size() {
	return (addr_t)&_mt_end - (addr_t)&_mt_begin; }


size_t Mode_transition_control::_master_context_size()
{
	addr_t const begin = (addr_t)&_mt_master_context_begin;
	addr_t const end = (addr_t)&_mt_master_context_end;
	return end - begin;
}


addr_t Mode_transition_control::_virt_user_entry()
{
	addr_t const phys      = (addr_t)&_mt_user_entry_pic;
	addr_t const phys_base = (addr_t)&_mt_begin;
	return VIRT_BASE + (phys - phys_base);
}


void Mode_transition_control::map(Genode::Translation_table * tt,
                                  Genode::Translation_table_allocator * alloc)
{
	try {
		addr_t const phys_base = (addr_t)&_mt_begin;
		tt->insert_translation(Genode::trunc_page(VIRT_BASE), phys_base, SIZE,
		                       Genode::Page_flags::mode_transition(), alloc);
	} catch(...) {
		Genode::error("inserting exception vector in page table failed!"); }
}


void Mode_transition_control::switch_to(Cpu::Context * const context,
                                        unsigned const cpu,
                                        addr_t const entry_raw,
                                        addr_t const context_ptr_base)
{
	/* override client-context pointer of the executing CPU */
	size_t const context_ptr_offset = cpu * sizeof(context);
	addr_t const context_ptr = context_ptr_base + context_ptr_offset;
	*(void * *)context_ptr = context;

	/* call assembly code that applies the virtual-machine context */
	typedef void (* Entry)();
	Entry __attribute__((noreturn)) const entry = (Entry)entry_raw;
	entry();
}


void Mode_transition_control::switch_to_user(Cpu::Context * const context,
                                             unsigned const cpu)
{
	switch_to(context, cpu, _virt_user_entry(),
	          (addr_t)&_mt_client_context_ptr);
}


Mode_transition_control::Mode_transition_control() : _master(&_table)
{
	assert(Genode::aligned(this, ALIGN_LOG2));
	assert(sizeof(_master) <= _master_context_size());
	assert(_size() <= SIZE);
	map(&_table, _alloc.alloc());
	Genode::memcpy(&_mt_master_context_begin, &_master, sizeof(_master));
}


Mode_transition_control * Kernel::mtc()
{
	typedef Mode_transition_control Control;
	return unmanaged_singleton<Control, Control::ALIGN>();
}
