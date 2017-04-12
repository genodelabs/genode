/*
 * \brief  NOVA-specific convenience functions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__NOVA_UTIL_H_
#define _CORE__INCLUDE__NOVA_UTIL_H_

/* Genode includes */
#include <base/log.h>

/* NOVA includes */
#include <nova/syscalls.h>

/* local includes */
#include <echo.h>
#include <util.h>

/**
 * Return boot CPU number. It is required if threads in core should be placed
 * on the same CPU as the main thread.
 */
inline Genode::addr_t boot_cpu()
{
	/**
	 * Initial value of ax and di register, saved by the crt0 startup code
	 * and SOLELY VALID in 'core' !!!
	 *
	 * For x86_32 - __initial_ax contains the number of the boot CPU.
	 * For x86_64 - __initial_di contains the number of the boot CPU.
	 */
	extern Genode::addr_t __initial_ax;
	extern Genode::addr_t __initial_di;

	return (sizeof(void *) > 4) ? __initial_di : __initial_ax;
}

/**
 * Establish a mapping
 *
 * \param utcb       UTCB of the calling EC
 * \param src_crd    capability range descriptor of source
 *                   resource to map locally
 * \param dst_crd    capability range descriptor of mapping
 *                   target
 * \param kern_pd    Whether to map the items from the kernel or from core
 * \param dma_mem    Whether the memory is usable for DMA or not
 *
 * This functions sends a message from the calling EC to the echo EC.
 * The calling EC opens a receive window and the echo EC creates a transfer
 * item of the message and replies. The kernel will map during the reply
 * from the echo EC to the calling EC.
 */
static int map_local(Nova::Utcb *utcb, Nova::Crd src_crd, Nova::Crd dst_crd,
                     bool kern_pd = false, bool dma_mem = false,
                     bool write_combined = false)
{
	/* open receive window at current EC */
	utcb->crd_rcv = dst_crd;

	/* tell echo thread what to map */
	utcb->msg()[0] = src_crd.value();
	utcb->msg()[1] = 0;
	utcb->msg()[2] = kern_pd;
	utcb->msg()[3] = dma_mem;
	utcb->msg()[4] = write_combined;
	utcb->set_msg_word(5);

	/* establish the mapping via a portal traversal during reply phase */
	Nova::uint8_t res = Nova::call(echo()->pt_sel());
	if (res != Nova::NOVA_OK || utcb->msg_words() != 1 || !utcb->msg()[0] ||
	    utcb->msg_items() != 1) {

		typedef Genode::Hex Hex;
		error("map_local failed ",
		      Hex(src_crd.addr()), ":", Hex(src_crd.order()), ":", Hex(src_crd.type()), "->",
		      Hex(dst_crd.addr()), ":", Hex(dst_crd.order()), ":", Hex(dst_crd.type()), " - ",
		      "result=", Hex(res), " "
		      "msg=",    Hex(utcb->msg_items()), ":",
		                 Hex(utcb->msg_words()), ":",
		                 Hex(utcb->msg()[0]), " !!! "
		      "utcb=",   utcb, " "
		      "kern=",   kern_pd);
		return res > 0 ? res : -1;
	}
	/* clear receive window */
	utcb->crd_rcv = 0;

	return 0;
}


static inline int unmap_local(Nova::Crd crd, bool self = true) {
	return Nova::revoke(crd, self); }

inline int map_local_phys_to_virt(Nova::Utcb *utcb, Nova::Crd src,
                                  Nova::Crd dst) {
	return map_local(utcb, src, dst, true); }

inline int map_local_one_to_one(Nova::Utcb *utcb, Nova::Crd crd) {
	return map_local(utcb, crd, crd, true); }


/**
 * Find least significant set bit in value
 */
inline unsigned char
lsb_bit(unsigned long const &value, unsigned char const shift = 0)
{
	unsigned long const scan  = value >> shift;
	if (scan == 0) return 0;

	unsigned char pos = __builtin_ctzl(scan);
	unsigned char res = shift ? pos + shift : pos;
	return res;
}

/**
 * Remap pages in the local address space
 *
 * \param utcb        UTCB of the main thread
 * \param from_start  physical source address
 * \param to_start    local virtual destination address
 * \param num_pages   number of pages to map
 */
inline int map_local(Nova::Utcb *utcb,
                     Genode::addr_t from_start, Genode::addr_t to_start,
                     Genode::size_t num_pages,
                     Nova::Rights const &permission,
                     bool kern_pd = false, bool dma_mem = false,
                     bool write_combined = false)
{
	using namespace Nova;
	using namespace Genode;

	size_t const size = num_pages << get_page_size_log2();

	addr_t const from_end = from_start + size;
	addr_t const to_end   = to_start   + size;

	for (addr_t offset = 0; offset < size; ) {

		addr_t const from_curr = from_start + offset;
		addr_t const to_curr   = to_start   + offset;

		/*
		 * The common alignment corresponds to the number of least significant
		 * zero bits in both addresses.
		 */
		addr_t const common_bits = from_curr | to_curr;

		/* find least set bit in common bits */
		size_t order = lsb_bit(common_bits, get_page_size_log2());

		/* look if flexpage fits into both 'from' and 'to' address range */
		if ((from_end - from_curr) < (1UL << order))
			order = log2(from_end - from_curr);

		if ((to_end - to_curr) < (1UL << order))
			order = log2(to_end - to_curr);

		int const res = map_local(utcb,
		                          Mem_crd((from_curr >> 12), order - get_page_size_log2(), permission),
		                          Mem_crd((to_curr   >> 12), order - get_page_size_log2(), permission),
		                          kern_pd, dma_mem, write_combined);
		if (res) return res;

		/* advance offset by current flexpage size */
		offset += (1UL << order);
	}
	return 0;
}


/**
 * Unmap pages from the local address space
 *
 * \param utcb        UTCB of the main thread
 * \param start       local virtual address
 * \param num_pages   number of pages to unmap
 * \param self        unmap from this pd or solely from other pds
 * \param self        map from this pd or solely from other pds
 * \param rights      rights to be revoked, default: all rwx
 */
inline void unmap_local(Nova::Utcb *utcb, Genode::addr_t start,
                        Genode::size_t num_pages,
                        bool const self = true,
                        Nova::Rights const rwx = Nova::Rights(true, true, true))
{
	using namespace Nova;
	using namespace Genode;

	Genode::addr_t base = start >> get_page_size_log2();

	if (start & (get_page_size() - 1)) {
		error("unmap failed - unaligned address specified");
		return;
	}

	while (num_pages) {
		unsigned char const base_bit  = lsb_bit(base);
		unsigned char const order_bit = min(log2(num_pages), 31U);
		unsigned char const order     = min(order_bit, base_bit);

		Mem_crd const crd(base, order, rwx);

		unmap_local(crd, self);

		num_pages -= 1UL << order;
		base      += 1UL << order;
	}
}


template <typename FUNC>
inline Nova::uint8_t syscall_retry(Genode::Pager_object &pager, FUNC func)
{
	Nova::uint8_t res;
	do {
		res = func();
	} while (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK == pager.handle_oom());

	return res;
}

inline Nova::uint8_t async_map(Genode::Pager_object &pager,
                               Genode::addr_t const source_pd,
                               Genode::addr_t const target_pd,
                               Nova::Obj_crd const &source_initial_caps,
                               Nova::Obj_crd const &target_initial_caps,
                               Nova::Utcb *utcb)
{
	/* asynchronously map capabilities */
	utcb->set_msg_word(0);

	/* ignore return value as one item always fits into the utcb */
	bool const ok = utcb->append_item(source_initial_caps, 0);
	(void)ok;

	return syscall_retry(pager,
		[&]() {
			return Nova::delegate(source_pd, target_pd, target_initial_caps);
		});
}

inline Nova::uint8_t map_vcpu_portals(Genode::Pager_object &pager,
                                      Genode::addr_t const source_exc_base,
                                      Genode::addr_t const target_exc_base,
                                      Nova::Utcb *utcb,
                                      Genode::addr_t const source_pd)
{
	using Nova::Obj_crd;
	using Nova::NUM_INITIAL_VCPU_PT_LOG2;

	Obj_crd const source_initial_caps(source_exc_base, NUM_INITIAL_VCPU_PT_LOG2);
	Obj_crd const target_initial_caps(target_exc_base, NUM_INITIAL_VCPU_PT_LOG2);

	return async_map(pager, source_pd, pager.pd_sel(),
	                 source_initial_caps, target_initial_caps, utcb);
}

inline Nova::uint8_t map_pagefault_portal(Genode::Pager_object &pager,
                                          Genode::addr_t const source_exc_base,
                                          Genode::addr_t const target_exc_base,
                                          Genode::addr_t const target_pd,
                                          Nova::Utcb *utcb)
{
	using Nova::Obj_crd;
	using Nova::PT_SEL_PAGE_FAULT;

	Genode::addr_t const source_pd = Genode::platform_specific()->core_pd_sel();

	Obj_crd const source_initial_caps(source_exc_base + PT_SEL_PAGE_FAULT, 0);
	Obj_crd const target_initial_caps(target_exc_base + PT_SEL_PAGE_FAULT, 0);

	return async_map(pager, source_pd, target_pd,
	                 source_initial_caps, target_initial_caps, utcb);
}

#endif /* _CORE__INCLUDE__NOVA_UTIL_H_ */
