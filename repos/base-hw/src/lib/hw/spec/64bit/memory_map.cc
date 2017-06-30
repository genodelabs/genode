/*
 * \brief  Memory map of core on 64bit
 * \author Stefan Kalkowski
 * \date   2017-08-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/native_utcb.h>

#include <hw/memory_map.h>

using Hw::Memory_region;
using Genode::addr_t;
using Genode::Native_utcb;

static constexpr addr_t USER_START = Genode::user_utcb_main_thread()
                                     + sizeof(Native_utcb);
static constexpr addr_t KERNEL_START = 0xffffffc000000000UL;

Memory_region const Hw::Mm::user() {
	return Memory_region(USER_START, 0x800000000000 - USER_START); }

Memory_region const Hw::Mm::core_heap() {
	return Memory_region(0xffffffd000000000UL, 0x1000000000UL); }

Memory_region const Hw::Mm::core_stack_area() {
	return Memory_region(0xffffffe000000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::core_page_tables() {
	return Memory_region(0xffffffe010000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::core_utcb_main_thread() {
	return Memory_region(0xffffffe020000000UL, sizeof(Native_utcb)); }

Memory_region const Hw::Mm::core_mmio() {
	return Memory_region(0xffffffe030000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::boot_info() {
	return Memory_region(0xffffffe040000000UL, 0x1000UL); }

Memory_region const Hw::Mm::supervisor_exception_vector() {
	return Memory_region(KERNEL_START, 0x1000UL); }
