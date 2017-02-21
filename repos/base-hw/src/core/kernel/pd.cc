/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/crt0.h>
#include <base/internal/unmanaged_singleton.h>
#include <base/log.h>
#include <hw/page_flags.h>
#include <hw/util.h>
#include <kernel/pd.h>
#include <util/construct_at.h>

#include <platform.h>

using namespace Kernel;
using Hw::Page_table;
using Genode::Platform;

/* structure of the mode transition */
extern int _mt_begin;
extern int _mt_user_entry_pic;
extern int _mt_client_context_ptr;


void Mode_transition_control::map(Page_table & tt,
                                  Page_table::Allocator & alloc)
{
	static addr_t const phys_base =
		Platform::core_phys_addr((addr_t)&_mt_begin);

	try {
		tt.insert_translation(Cpu::exception_entry, phys_base, Cpu::mtc_size,
		                      Hw::PAGE_FLAGS_KERN_EXCEP, alloc);
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
	static addr_t entry = (addr_t)Cpu::exception_entry
	                      + ((addr_t)&_mt_user_entry_pic
	                         - (addr_t)&_mt_begin);
	switch_to(context, cpu, entry, (addr_t)&_mt_client_context_ptr);
}


Mode_transition_control * Kernel::mtc() {
	return unmanaged_singleton<Mode_transition_control>(); }
