/*
 * \brief   Initial pool of untyped memory
 * \author  Norman Feske
 * \date    2016-02-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_
#define _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_

/* Genode includes */
#include <base/exception.h>

/* core includes */
#include <types.h>
#include <sel4_boot_info.h>

/* seL4 includes */
#include <sel4/sel4.h>

namespace Core { class Initial_untyped_pool; }


class Core::Initial_untyped_pool
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
			unsigned const index = (unsigned)(sel - sel4_boot_info().untyped.start);

			/* original size of untyped memory range */
			size_t const size = 1UL << sel4_boot_info().untypedList[index].sizeBits;

			/* physical address of the begin of the untyped memory range */
			addr_t const phys = sel4_boot_info().untypedList[index].paddr;

			bool const device = sel4_boot_info().untypedList[index].isDevice;

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
		addr_t _align_offset(Range &range, unsigned size_log2)
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

		/**
		 * Apply functor to each untyped memory range
		 *
		 * The functor is called with 'Range &' as argument.
		 */
		template <typename FUNC>
		void for_each_range(FUNC const &func)
		{
			seL4_BootInfo const &bi = sel4_boot_info();
			for (addr_t sel = bi.untyped.start; sel < bi.untyped.end; sel++) {
				Range range(*this, (unsigned)sel);
				func(range);
			}
		}

	public:

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
		unsigned alloc(unsigned size_log2)
		{
			enum { UNKNOWN = 0 };
			unsigned sel = UNKNOWN;

			/*
			 * Go through the known initial untyped memory ranges to find
			 * a range that is able to host a kernel object of 'size'.
			 */
			for_each_range([&] (Range &range) {
				/* ignore device memory */
				if (range.device)
					return;

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
		 * Convert (remainder) of the initial untyped memory into untyped
		 * objects of size_log2 and up to a maximum as specified by max_memory
		 */
		template <typename FUNC>
		void turn_into_untyped_object(addr_t  const node_index,
		                              FUNC    const & func,
		                              size_t  const size_log2 = get_page_size_log2(),
		                              addr_t  max_memory = 0UL - 0x1000UL)
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
						align_addr(range.free_offset, (int)size_log2);

					/* back out if no further page can be allocated */
					if (page_aligned_free_offset + (1UL << size_log2) > range.size)
						return;

					if (!max_memory)
						return;

					size_t const remaining_size    = range.size - page_aligned_free_offset;
					size_t const retype_size_limit = get_page_size()*256;
					size_t const batch_size        = min(min(remaining_size, retype_size_limit), max_memory);

					addr_t const phys_addr = range.phys + page_aligned_free_offset;
					size_t const num_pages = batch_size / (1UL << size_log2);

					seL4_Untyped const service     = range.sel;
					addr_t       const type        = seL4_UntypedObject;
					addr_t       const size_bits   = size_log2;
					seL4_CNode   const root        = Core_cspace::top_cnode_sel();
					addr_t       const node_depth  = Core_cspace::NUM_TOP_SEL_LOG2;
					addr_t       const node_offset = phys_addr >> size_log2;
					addr_t       const num_objects = num_pages;

					/* XXX skip memory because of limited untyped cnode range */
					if (node_offset >= (1UL << (32 - get_page_size_log2()))) {
						warning(range.device ? "device" : "      ", " memory in range ",
						        Hex_range<addr_t>(range.phys, range.size),
						        " is unavailable (due to limited untyped cnode range)");
						return;
					}

					/* invoke callback about the range */
					bool const used = func(phys_addr, num_pages << size_log2,
					                       range.device);

					if (!used)
						return;

					long const ret = seL4_Untyped_Retype(service,
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

					/* mark consumed untyped memory range as allocated */
					range.free_offset += batch_size;

					/* track memory left to be converted */
					max_memory -= batch_size;
				}
			});
		}
};

#endif /* _CORE__INCLUDE__INITIAL_UNTYPED_POOL_H_ */
