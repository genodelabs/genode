/*
 * \brief  Memory map of core on ARM 32-bit
 * \author Stefan Kalkowski
 * \date   2017-06-21
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/native_utcb.h>

#include <hw/memory_consts.h>
#include <hw/memory_map.h>

using Hw::Memory_region;
using Genode::addr_t;
using Genode::Native_utcb;

static constexpr addr_t USER_START = Genode::user_utcb_main_thread()
                                     + sizeof(Native_utcb);

Memory_region const Hw::Mm::user() {
	return Memory_region(USER_START, KERNEL_START - USER_START); }

Memory_region const Hw::Mm::core_heap() {
	return Memory_region(0xa0000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::core_stack_area() {
	return Memory_region(0xb0000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::core_page_tables() {
	return Memory_region(0xc0000000UL, 0x10000000UL); }

Memory_region const Hw::Mm::cpu_local_memory() {
	return Memory_region(CPU_LOCAL_MEMORY_AREA_START,
	                     CPU_LOCAL_MEMORY_AREA_SIZE); }

Memory_region const Hw::Mm::core_mmio() {
	return Memory_region(0xf0000000UL, 0xf000000UL); }

Memory_region const Hw::Mm::system_exception_vector() {
	return Memory_region(0xfff00000UL, 0x1000UL); }

Memory_region const Hw::Mm::hypervisor_exception_vector() {
	return Memory_region(0xfff10000UL, 0x1000UL); }

Memory_region const Hw::Mm::hypervisor_stack() {
	return Memory_region(0xfff20000UL, 0x10000UL); }

Memory_region const Hw::Mm::boot_info() {
	return Memory_region(0xfffe0000UL, 0x1000UL); }

Memory_region const Hw::Mm::core_utcb_main_thread() {
	return Memory_region(0xfffef000, sizeof(Native_utcb)); }

Memory_region const Hw::Mm::supervisor_exception_vector() {
	return Memory_region(0xffff0000UL, 0x1000UL); }
