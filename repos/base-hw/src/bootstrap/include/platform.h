/*
 * \brief  Platform interface
 * \author Stefan Kalkowski
 * \date   2016-10-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/internal/elf.h>
#include <util/reconstructible.h>

/* core includes */
#include <bootinfo.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>
#include <rom_fs.h>

using Genode::addr_t;
using Genode::size_t;
using Genode::Bootinfo;
using Genode::Core_mmio;
using Genode::Mapping;
using Genode::Memory_region;
using Genode::Memory_region_array;
using Genode::Rom_module;


class Platform
{
	private:

		struct Board : Genode::Board
		{
			Memory_region_array early_ram_regions;
			Memory_region_array late_ram_regions;
			Core_mmio const     core_mmio;

			Board();
		};


		class Ram_allocator : public Genode::Allocator_avl_base
		{
			private:

				using Base = Genode::Allocator_avl_base;

				enum { BSZ = Genode::Allocator_avl::slab_block_size() };

				Genode::Tslab<Base::Block, BSZ> _slab;
				Genode::uint8_t                 _first_slab[BSZ];

			public:

				Ram_allocator()
				: Genode::Allocator_avl_base(&_slab, sizeof(Base::Block)),
				  _slab(this, (Genode::Slab_block*)&_first_slab) {}

				void * alloc_aligned(size_t size, unsigned align);
				bool   alloc(size_t size, void **out_addr) override;
				void * alloc(size_t size) { return Allocator::alloc(size); }

				void add(Memory_region const &);
				void remove(Memory_region const &);

				template <typename FUNC>
				void for_each_free_region(FUNC functor)
				{
					_block_tree().for_each([&] (Block const & b)
					{
						if (!b.used())
							functor(Genode::Memory_region(b.addr(), b.size()));
					});
				}
		};


		struct Pd
		{
			void * const                table_base;
			void * const                allocator_base;
			Bootinfo::Table           & table;
			Bootinfo::Table_allocator & allocator;
			Bootinfo::Mapping_pool      mappings;

			Pd(Ram_allocator & alloc);

			void map(Mapping m);
			void map_insert(Mapping m);
		};


		struct Elf : Genode::Elf_binary
		{
			Elf(addr_t const addr) : Genode::Elf_binary(addr) { }

			template <typename T> void for_each_segment(T functor)
			{
				Genode::Elf_segment segment;
				for (unsigned i = 0; (segment = get_segment(i)).valid(); i++) {
					if (segment.flags().skip)    continue;
					if (segment.mem_size() == 0) continue;
					functor(segment);
				}
			}
		};

		Board                     board;
		Genode::Cpu               cpu;
		Genode::Pic               pic;
		Ram_allocator             ram_alloc;
		Memory_region const       bootstrap_region;
		Genode::Constructible<Pd> core_pd;
		addr_t                    core_elf_addr;
		Elf                       core_elf;

		addr_t _load_elf();

	public:

		Platform();

		static addr_t mmio_to_virt(addr_t mmio) { return mmio; }

		void enable_mmu();
		void start_core() __attribute__((noreturn));
};

extern Platform & platform();

#endif /* _PLATFORM_H_ */
