/*
 * \brief  NOVA-specific convenience functions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOVA_UTIL_H_
#define _NOVA_UTIL_H_

/* Genode includes */
#include <base/printf.h>

/* NOVA includes */
#include <nova/syscalls.h>

/* local includes */
#include <echo.h>
#include <util.h>

enum { verbose_local_map = false };


/**
 * Establish a one-to-one mapping
 *
 * \param utcb       UTCB of the calling EC
 * \param src_crd    capability range descriptor of source
 *                   resource to map locally
 * \param dst_crd    capability range descriptor of mapping
 *                   target
 *
 * This functions sends a mapping from the calling EC to the echo EC.
 * In order to successfully transfer the mapping, we have to open a
 * corresponding receive window at the echo EC.  We do this by poking
 * a receive-capability-range descriptor directly onto the echo UTCB.
 */
static int map_local(Nova::Utcb *utcb, Nova::Crd src_crd, Nova::Crd dst_crd,
                     bool kern_pd = false)
{
	/* open receive window at current EC */
	utcb->crd_rcv = dst_crd;

	/* tell echo thread what to map */
	utcb->msg[0] = src_crd.value();
	utcb->msg[1] = 0;
	utcb->msg[2] = kern_pd;
	utcb->set_msg_word(3);

	/* establish the mapping via a portal traversal during reply phase */
	Nova::uint8_t res = Nova::call(echo()->pt_sel());
	if (res != Nova::NOVA_OK || utcb->msg_words() != 1 || !utcb->msg[0]) {
		PERR("Failure - map_local 0x%lx:%lu:%u->0x%lx:%lu:%u - call result=%x"
		     " utcb=%x:%lx !!! %p %u",
		     src_crd.addr(), src_crd.order(), src_crd.type(),
		     dst_crd.addr(), dst_crd.order(), dst_crd.type(),
		     res, utcb->msg_words(), utcb->msg[0], utcb, kern_pd);
		return res > 0 ? res : -1;
	}
	/* clear receive window */
	utcb->crd_rcv = 0;

	return 0;
}


static inline int unmap_local(Nova::Crd crd, bool self = true) {
	return Nova::revoke(crd, self); }

inline int
map_local_phys_to_virt(Nova::Utcb *utcb, Nova::Crd src, Nova::Crd dst) {
	return map_local(utcb, src, dst, true); }

inline int
map_local_one_to_one(Nova::Utcb *utcb, Nova::Crd crd) {
	return map_local(utcb, crd, crd, true); }


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
                     bool kern_pd = false)
{
	if (verbose_local_map)
		Genode::printf("::map_local: from %lx to %lx, %zd pages from kernel %u\n",
		               from_start, to_start, num_pages, kern_pd);

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

		/*
		 * Find highest clear bit in 'diff', starting from the least
		 * significant candidate. We can skip all bits lower then
		 * 'get_page_size_log2()' because they are not relevant as flexpage
		 * size (and are always zero).
		 */
		size_t order = get_page_size_log2();
		for (; order < 32 && !(common_bits & (1UL << order)); order++);

		/*
		 * Look if flexpage fits into both 'from' and 'to' address range
		 */

		if ((from_end - from_curr) < (1UL << order))
			order = log2(from_end - from_curr);

		if ((to_end - to_curr) < (1UL << order))
			order = log2(to_end - to_curr);

		if (verbose_local_map)
			Genode::printf("::map_local: order %zx %lx:%lx %lx:%lx\n",
			               order, from_curr, from_end, to_curr, to_end);

		int const res = map_local(utcb,
		                          Mem_crd((from_curr >> 12), order - get_page_size_log2(), permission),
		                          Mem_crd((to_curr   >> 12), order - get_page_size_log2(), permission),
		                          kern_pd);
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
 */
inline void unmap_local(Nova::Utcb *utcb,
                        Genode::addr_t start,
                        Genode::size_t num_pages)
{
	if (verbose_local_map)
		Genode::printf("::unmap_local: from %lx, %zd pages\n",
		               start, num_pages);

	using namespace Nova;
	using namespace Genode;
	Rights const rwx(true, true, true);

	Genode::addr_t end = start + (num_pages << get_page_size_log2()) - 1;

	while (true) {
		Nova::Mem_crd crd(start >> 12, 32, rwx);
		Nova::lookup(crd);

		if (!crd.is_null()) {

			if (verbose_local_map)
				PINF("Unmapping local: %08lx base: %lx order: %lx size: %lx is null: %d",
				     start, crd.base(), crd.order(),
				     (0x1000UL << crd.order()), crd.is_null());

			unmap_local(crd, true);

			start = (crd.base() << 12)       /* base address of mapping */
			      + (0x1000 << crd.order()); /* size of mapping */
		} else {

			/* This can happen if the region has never been touched */

			if (verbose_local_map)
				PINF("Nothing mapped at local: %08lx", start);

			start += 0x1000;
		}

		if (start > end)
			return;
	}
}


#endif /* _NOVA_UTIL_H_ */
