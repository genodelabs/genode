/*
 * \brief  Generic Translation lookaside buffer interface
 * \author Martin Stein
 * \date   2010-11-08
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INCLUDE__GENERIC__TLB_H_
#define _KERNEL__INCLUDE__GENERIC__TLB_H_

#include <kernel/types.h>
#include <xilinx/microblaze.h>

namespace Kernel {

	template <typename DEV_T>
	class Tlb_tpl : public DEV_T
	{

		private:

			typedef typename DEV_T::Entry_id Entry_id;

			Entry_id _current_entry_id;

			static Entry_id const fixed_entry_id_1 = 0;
			static Entry_id const fixed_entry_id_2 = 1;

			void _next_entry_id()
			{
				_current_entry_id++;
				if (_current_entry_id >= DEV_T::max_entry_id()) {
					_current_entry_id = 0;
				}
			}

		public:

			typedef Paging::Virtual_page  Virtual_page;
			typedef Paging::Physical_page Physical_page;
			typedef Paging::Resolution    Resolution;

			Tlb_tpl() : _current_entry_id(0) { }

			/**
			 * Add resolution to the tlb (not persistent)
			 */
			void add(Resolution* r)
			{
				if (!r->valid()) {
					printf("Error in Kernel::Tlb::add, invalid page\n");
				}

				while (1) {
					if (!fixed(_current_entry_id)) { break; }
					else { _next_entry_id(); }
				}

				if(DEV_T::set_entry(_current_entry_id, 
				                    r->physical_page.address(), r->virtual_page.address(),
				                    r->virtual_page.protection_id(), 
				                    Paging::size_log2_by_physical_page_size[r->physical_page.size()],
				                    r->physical_page.permissions() == Paging::Physical_page::RW ||
				                    r->physical_page.permissions() == Paging::Physical_page::RWX,
				                    r->physical_page.permissions() == Paging::Physical_page::RX ||
				                    r->physical_page.permissions() == Paging::Physical_page::RWX))
				{
					PERR("Writing to TLB failed");
				}
				_next_entry_id();
			}

			/**
			 * Add fixed resolution to the tlb (persistent till overwritten by
			 * fixed resolution)
			 */
			void add_fixed(Resolution* r1, Resolution* r2)
			{
				if(DEV_T::set_entry(fixed_entry_id_1, 
				                    r1->physical_page.address(), r1->virtual_page.address(),
				                    r1->virtual_page.protection_id(), 
				                    Paging::size_log2_by_physical_page_size[r1->physical_page.size()],
				                    r1->physical_page.permissions() == Paging::Physical_page::RW ||
				                    r1->physical_page.permissions() == Paging::Physical_page::RWX,
				                    r1->physical_page.permissions() == Paging::Physical_page::RX ||
				                    r1->physical_page.permissions() == Paging::Physical_page::RWX)) 
				{
					PERR("Writing to TLB failed");
				}

				if(DEV_T::set_entry(fixed_entry_id_2, 
				                    r2->physical_page.address(), r2->virtual_page.address(),
				                    r2->virtual_page.protection_id(), 
				                    Paging::size_log2_by_physical_page_size[r2->physical_page.size()],
				                    r2->physical_page.permissions() == Paging::Physical_page::RW ||
				                    r2->physical_page.permissions() == Paging::Physical_page::RWX,
				                    r2->physical_page.permissions() == Paging::Physical_page::RX ||
				                    r2->physical_page.permissions() == Paging::Physical_page::RWX))
				{
					PERR("Writing to TLB failed");
				}
			}

			bool fixed(Entry_id i) {
				return (i == fixed_entry_id_1) || (i == fixed_entry_id_2);
			}

			void flush(Virtual_page *base, unsigned size);
	};

	typedef Tlb_tpl<Xilinx::Microblaze::Mmu> Tlb;

	/**
	 * Pointer to kernels static translation lookaside buffer
	 */
	Tlb * tlb();
}


template <typename DEV_T>
void Kernel::Tlb_tpl<DEV_T>::flush(Virtual_page *base, unsigned size)
{
	addr_t area_base = base->address();
	addr_t area_top  = area_base + size;

	for (Entry_id i=0; i <= DEV_T::MAX_ENTRY_ID; i++) {

		if (fixed(i)) { continue; }

		Cpu::addr_t page;
		Protection_id pid;
		unsigned size_log2;

		if(DEV_T::get_entry(i, page, pid, size_log2)) {
			PERR("Reading TLB entry failed");
		}
		if (base->protection_id() != pid) { continue; }

		if(page < area_top && (page + (1<<size_log2)) > area_base) {
			DEV_T::clear_entry(i);
		}
	}
}


#endif /* _KERNEL__INCLUDE__GENERIC__TLB_H_ */
