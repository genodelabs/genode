/*
 * \brief  NOVA-specific convenience functions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
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
	/* open receive window at the echo EC */
	echo()->utcb()->crd_rcv = dst_crd;

	/* reset message transfer descriptor */
	utcb->set_msg_word(0);

	/* append capability-range as message-transfer item */
	utcb->append_item(src_crd, 0, kern_pd);

	/* establish the mapping via a portal traversal */
	if (echo()->pt_sel() == 0)
		PWRN("call to pt 0");
	return Nova::call(echo()->pt_sel());
}


static inline int unmap_local(Nova::Crd crd, bool self = true) {
	return Nova::revoke(crd, self); }


inline int map_local_one_to_one(Nova::Utcb *utcb, Nova::Crd crd) {
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
                     bool kern_pd = false)
{
	if (verbose_local_map)
		Genode::printf("::map_local: from %lx to %lx, %zd pages from kernel %u\n",
		               from_start, to_start, num_pages, kern_pd);

	using namespace Nova;
	using namespace Genode;
	Rights const rwx(true, true, true);

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
		for (; order < 32 && !(common_bits & (1 << order)); order++);

		/*
		 * Look if flexpage fits into both 'from' and 'to' address range
		 */

		if (from_curr + (1 << order) > from_end)
			order = log2(from_end - from_curr);

		if (to_curr + (1 << order) > to_end)
			order = log2(to_end - to_curr);

		int const res = map_local(utcb,
		                          Mem_crd((from_curr >> 12), order - get_page_size_log2(), rwx),
		                          Mem_crd((to_curr   >> 12), order - get_page_size_log2(), rwx),
		                          kern_pd);
		if (res) return res;

		/* advance offset by current flexpage size */
		offset += (1 << order);
	}
	return 0;
}


#endif /* _NOVA_UTIL_H_ */
