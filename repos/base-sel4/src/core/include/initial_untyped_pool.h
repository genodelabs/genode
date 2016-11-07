/*
 * \brief   Initial pool of untyped memory
 * \author  Norman Feske
 * \date    2016-02-11
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_
#define _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_

/* Genode includes */
#include <base/exception.h>
#include <base/log.h>

/* core-local includes */
#include <sel4_boot_info.h>

/* seL4 includes */
#include <sel4/sel4.h>

namespace Genode { class Initial_untyped_pool; }


class Genode::Initial_untyped_pool
{
	private:

		/* base limit on sel4's autoconf.h */
		enum { MAX_UNTYPED = (unsigned)CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS };

		struct Free_offset { addr_t value = 0; };

		Free_offset _free_offset[MAX_UNTYPED];

	public:

		class Initial_untyped_pool_exhausted : Exception { };

		struct Range
		{
			/* core-local cap selector */
			unsigned const sel;

			/* index into 'untypedSizeBitsList' */
			unsigned const index = sel - sel4_boot_info().untyped.start;

			/* original size of untyped memory range */
			size_t const size = 1UL << sel4_boot_info().untypedSizeBitsList[index];

			/* physical address of the begin of the untyped memory range */
			addr_t const phys = sel4_boot_info().untypedPaddrList[index];

			/* offset to the unused part of the untyped memory range */
			addr_t &free_offset;

			Range(Initial_untyped_pool &pool, unsigned sel)
			:
				sel(sel), free_offset(pool._free_offset[index].value)
			{ }
		};

	private:

		/**
		 * Calculate free index after allocation
		 */
		addr_t _align_offset(Range &range, size_t size_log2)
		{
			/*
			 * The seL4 kernel naturally aligns allocations within untuped
			 * memory ranges. So we have to apply the same policy to our
			 * shadow version of the kernel's 'FreeIndex'.
			 */
			addr_t const aligned_free_offset = align_addr(range.free_offset,
			                                              size_log2);

			return aligned_free_offset + (1 << size_log2);
		}

	public:

		/**
		 * Apply functor to each untyped memory range
		 *
		 * The functor is called with 'Range &' as argument.
		 */
		template <typename FUNC>
		void for_each_range(FUNC const &func)
		{
			seL4_BootInfo const &bi = sel4_boot_info();
			for (unsigned sel = bi.untyped.start; sel < bi.untyped.end; sel++) {
				Range range(*this, sel);
				func(range);
			}
		}


		/**
		 * Return selector of untyped memory range where the allocation of
		 * the specified size is possible
		 *
		 * \param kernel object size
		 *
		 * This function models seL4's allocation policy of untyped memory. It
		 * is solely used at boot time to setup core's initial kernel objects
		 * from the initial pool of untyped memory ranges as reported by the
		 * kernel.
		 *
		 * \throw Initial_untyped_pool_exhausted
		 */
		unsigned alloc(size_t size_log2)
		{
			enum { UNKNOWN = 0 };
			unsigned sel = UNKNOWN;

			/*
			 * Go through the known initial untyped memory ranges to find
			 * a range that is able to host a kernel object of 'size'.
			 */
			for_each_range([&] (Range &range) {

				/* calculate free index after allocation */
				addr_t const new_free_offset = _align_offset(range, size_log2);

				/* check if allocation fits within current untyped memory range */
				if (new_free_offset > range.size)
					return;

				if (sel == UNKNOWN) {
					sel = range.sel;
					return;
				}

				/* check which range is smaller - take that */
				addr_t const rest = range.size - new_free_offset;

				Range best_fit(*this, sel);
				addr_t const new_free_offset_best = _align_offset(best_fit, size_log2);
				addr_t const rest_best = best_fit.size - new_free_offset_best;

				if (rest_best >= rest)
					/* current range fits better then best range */
					sel = range.sel;
			});

			if (sel == UNKNOWN) {
				warning("Initial_untyped_pool exhausted");
				throw Initial_untyped_pool_exhausted();
			}

			Range best_fit(*this, sel);
			addr_t const new_free_offset = _align_offset(best_fit, size_log2);
			ASSERT(new_free_offset <= best_fit.size);

			/*
			 * We found a matching range, consume 'size' and report the
			 * selector. The returned selector is used by the caller
			 * of 'alloc' to perform the actual kernel-object creation.
			 */
			best_fit.free_offset = new_free_offset;

			return best_fit.sel;
		}

		/**
		 * Convert remainder of the initial untyped memory into untyped pages
		 */
		void turn_remainder_into_untyped_pages()
		{
			for_each_range([&] (Range &range) {

				/*
				 * The kernel limits the maximum number of kernel objects to
				 * be created via a single untyped-retype operation. So we
				 * need to iterate for each range, converting a limited batch
				 * of pages in each step.
				 */
				for (;;) {

					addr_t const page_aligned_free_offset =
						align_addr(range.free_offset, get_page_size_log2());

					/* back out if no further page can be allocated */
					if (page_aligned_free_offset + get_page_size() > range.size)
						return;

					size_t const remaining_size    = range.size - page_aligned_free_offset;
					size_t const retype_size_limit = get_page_size()*256;
					size_t const batch_size        = min(remaining_size, retype_size_limit);

					/* mark consumed untyped memory range as allocated */
					range.free_offset += batch_size;

					addr_t const phys_addr = range.phys + page_aligned_free_offset;
					size_t const num_pages = batch_size / get_page_size();

					seL4_Untyped const service     = range.sel;
					int          const type        = seL4_UntypedObject;
					int          const size_bits   = get_page_size_log2();
					seL4_CNode   const root        = Core_cspace::top_cnode_sel();
					int          const node_index  = Core_cspace::TOP_CNODE_UNTYPED_IDX;
					int          const node_depth  = Core_cspace::NUM_TOP_SEL_LOG2;
					int          const node_offset = phys_addr >> get_page_size_log2();
					int          const num_objects = num_pages;

					int const ret = seL4_Untyped_Retype(service,
					                                    type,
					                                    size_bits,
					                                    root,
					                                    node_index,
					                                    node_depth,
					                                    node_offset,
					                                    num_objects);

					if (ret != 0) {
						error(__func__, ": seL4_Untyped_Retype (untyped) "
						      "returned ", ret);
						return;
					}
				}
			});
		}
};

#endif /* _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_ */
