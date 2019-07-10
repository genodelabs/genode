/*
 * \brief  CPU cache maintenance functions for ARM v7
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <spec/arm/cpu.h>

/**
 * Helpers that increase readability of MCR and MRC commands
 */
#define READ_CLIDR(rd)   "mrc p15, 1, " #rd ", c0, c0, 1\n"
#define READ_CCSIDR(rd)  "mrc p15, 1, " #rd ", c0, c0, 0\n"
#define WRITE_CSSELR(rs) "mcr p15, 2, " #rs ", c0, c0, 0\n"
#define WRITE_DCISW(rs)  "mcr p15, 0, " #rs ", c7, c6, 2\n"
#define WRITE_DCCSW(rs)  "mcr p15, 0, " #rs ", c7, c10, 2\n"

/**
 * First macro to do a set/way operation on all entries of all data caches
 *
 * Must be inserted directly before the targeted operation. Returns operand
 * for targeted operation in R6.
 */
#define FOR_ALL_SET_WAY_IN_R6_0 \
 \
	/* get the cache level value (Clidr::Loc) */ \
	READ_CLIDR(r0) \
	"ands r3, r0, #0x7000000\n" \
	"mov r3, r3, lsr #23\n" \
 \
	/* skip all if cache level value is zero */ \
	"beq 5f\n" \
	"mov r9, #0\n" \
 \
	/* begin loop over cache numbers */ \
	"1:\n" \
 \
	/* work out 3 x cache level */ \
	"add r2, r9, r9, lsr #1\n" \
 \
	/* get the cache type of current cache number (Clidr::CtypeX) */ \
	"mov r1, r0, lsr r2\n" \
	"and r1, r1, #7\n" \
	"cmp r1, #2\n" \
 \
	/* skip cache number if there's no data cache at this level */ \
	"blt 4f\n" \
 \
	/* select the appropriate CCSIDR according to cache level and type */ \
	WRITE_CSSELR(r9) \
	"isb\n" \
 \
	/* get the line length of current cache (Ccsidr::LineSize) */ \
	READ_CCSIDR(r1) \
	"and r2, r1, #0x7\n" \
 \
	/* add 4 for the line-length offset (log2 of 16 bytes) */ \
	"add r2, r2, #4\n" \
 \
	/* get the associativity or max way size (Ccsidr::Associativity) */ \
	"ldr r4, =0x3ff\n" \
	"ands r4, r4, r1, lsr #3\n" \
 \
	/* get the bit position of the way-size increment */ \
	"clz r5, r4\n" \
 \
	/* get a working copy of the max way size */ \
	"mov r8, r4\n" \
 \
	/* begin loop over way numbers */ \
	"2:\n" \
 \
	/* get the number of sets or the max index size (Ccsidr::NumSets) */ \
	"ldr r7, =0x00007fff\n" \
	"ands r7, r7, r1, lsr #13\n" \
 \
	/* begin loop over indices */ \
	"3:\n" \
 \
	/* factor in the way number and cache number into write value */ \
	"orr r6, r9, r8, lsl r5\n" \
 \
	/* factor in the index number into write value */ \
	"orr r6, r6, r7, lsl r2\n"

/**
 * Second macro to do a set/way operation on all entries of all data caches
 *
 * Must be inserted directly after the targeted operation.
 */
#define FOR_ALL_SET_WAY_IN_R6_1 \
 \
	/* decrement the index */ \
	"subs r7, r7, #1\n" \
 \
	/* end loop over indices */ \
	"bge 3b\n" \
 \
	/* decrement the way number */ \
	"subs r8, r8, #1\n" \
 \
	/* end loop over way numbers */ \
	"bge 2b\n" \
 \
	/* label to skip a cache number */ \
	"4:\n" \
 \
	/* increment the cache number */ \
	"add r9, r9, #2\n" \
	"cmp r3, r9\n" \
 \
	/* end loop over cache numbers */ \
	"bgt 1b\n" \
 \
	/* synchronize data */ \
	"dsb\n" \
 \
	/* label to skip all */ \
	"5:\n" \
	::: "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9"


void Board::Cpu::invalidate_data_cache()
{
	/**
	 * Data Cache Invalidate by Set/Way for all Set/Way
	 */
	asm volatile (FOR_ALL_SET_WAY_IN_R6_0
	              WRITE_DCISW(r6)
	              FOR_ALL_SET_WAY_IN_R6_1);
}


void Board::Cpu::clean_invalidate_data_cache()
{
	/**
	 * Data Cache Clean by Set/Way for all Set/Way
	 */
	asm volatile (FOR_ALL_SET_WAY_IN_R6_0
	              WRITE_DCCSW(r6)
	              FOR_ALL_SET_WAY_IN_R6_1);
}
