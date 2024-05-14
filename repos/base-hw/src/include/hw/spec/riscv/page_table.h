/*
 * \brief  RISCV Sv39 page table format
 * \author Sebastian Sumpf
 * \date   2015-08-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_
#define _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_

#include <base/log.h>
#include <cpu/page_flags.h>
#include <hw/page_table_allocator.h>
#include <util/misc_math.h>
#include <util/register.h>

namespace Sv39 {

	using namespace Genode;

	enum {
		SIZE_LOG2_4K   = 12,
		SIZE_LOG2_2M   = 21,
		SIZE_LOG2_1G   = 30,
		SIZE_LOG2_512G = 39,
	};

	struct None { };

	template <typename ENTRY, unsigned BLOCK_SIZE_LOG2, unsigned SIZE_LOG2>
	class Level_x_translation_table;

	using Level_3_translation_table =
		Level_x_translation_table<None, SIZE_LOG2_4K, SIZE_LOG2_2M>;

	using Level_2_translation_table =
		Level_x_translation_table<Level_3_translation_table, SIZE_LOG2_2M, SIZE_LOG2_1G>;

	using Level_1_translation_table =
		Level_x_translation_table<Level_2_translation_table, SIZE_LOG2_1G, SIZE_LOG2_512G>;

	struct Descriptor;
	struct Table_descriptor;
	struct Block_descriptor;
}


struct Sv39::Descriptor : Register<64>
{
	enum Descriptor_type { INVALID, TABLE, BLOCK };
	struct V : Bitfield<0, 1> { }; /* present */
	struct R : Bitfield<1, 1> { }; /* read */
	struct W : Bitfield<2, 1> { }; /* write */
	struct X : Bitfield<3, 1> { }; /* executable */
	struct U : Bitfield<4, 1> { }; /* user */
	struct G : Bitfield<5, 1> { }; /* global  */
	struct A : Bitfield<6, 1> { }; /* access bit */
	struct D : Bitfield<7, 1> { }; /* dirty bit */

	struct Perm : Bitfield<0, 5> { };
	struct Type : Bitfield<1, 3>
	{
		enum {
			POINTER = 0
		};
	};
	struct Ppn  : Bitfield<10, 38> { }; /* physical address 10 bit aligned */
	struct Base : Bitfield<12, 38> { }; /* physical address page aligned */

	static access_t permission_bits(Genode::Page_flags const &f)
	{
		access_t rights = 0;
		R::set(rights, 1);

		if (f.writeable)
			W::set(rights, 1);

		if (f.executable)
			X::set(rights, 1);

		if (!f.privileged)
			U::set(rights, 1);

		if (f.global)
			G::set(rights, 1);

		return rights;
	}

	static Descriptor_type type(access_t const v)
	{
		if (!V::get(v)) return INVALID;
		if (Type::get(v) == Type::POINTER)
			return TABLE;

		return BLOCK;
	}

	static bool valid(access_t const v) {
		return V::get(v); }
};


struct Sv39::Table_descriptor : Descriptor
{
	static access_t create(void * const pa)
	{
		access_t base = Base::get((access_t)pa);
		access_t desc = 0;

		Ppn::set(desc, base);
		Type::set(desc, Type::POINTER);
		V::set(desc, 1);

		return desc;
	}
};


struct Sv39::Block_descriptor : Descriptor
{
	static access_t create(Genode::Page_flags const &f, addr_t const pa)
	{
		access_t base = Base::get(pa);
		access_t desc = 0;

		Ppn::set(desc, base);
		Perm::set(desc, permission_bits(f));

		/*
		 * Always set access and dirty bits because RISC-V may raise a page fault
		 * (implementation dependend) in case it observes this bits being cleared.
		 */
		A::set(desc, 1);
		if (f.writeable)
			D::set(desc, 1);

		V::set(desc, 1);

		return desc;
	}
};


template <typename ENTRY, unsigned BLOCK_SIZE_LOG2, unsigned SIZE_LOG2>
class Sv39::Level_x_translation_table
{
	private:

		bool _aligned(addr_t const a, size_t const alignm_log2) {
			return a == ((a >> alignm_log2) << alignm_log2); }

		void _translation_added(addr_t, size_t);

	public:

		static constexpr size_t MIN_PAGE_SIZE_LOG2 = SIZE_LOG2_4K;
		static constexpr size_t ALIGNM_LOG2        = SIZE_LOG2_4K;
		static constexpr size_t MAX_ENTRIES        = 1 << (SIZE_LOG2 - BLOCK_SIZE_LOG2);
		static constexpr size_t BLOCK_SIZE         = 1 << BLOCK_SIZE_LOG2;
		static constexpr size_t BLOCK_MASK         = ~(BLOCK_SIZE - 1);
		static constexpr size_t VM_MASK            = (1UL<< SIZE_LOG2_512G) - 1;

		class Misaligned { };
		class Invalid_range { };
		class Double_insertion { };

		using Allocator = Hw::Page_table_allocator<4096>;

	protected:

		typename Descriptor::access_t _entries[MAX_ENTRIES];

		/*
		 * Return how many entries of an alignment fit into region
		 */
		static constexpr size_t _count(size_t region, size_t alignment) {
			return align_addr<size_t>(region, (unsigned)alignment) / (1UL << alignment); }


		template <typename FUNC>
		void _range_op(addr_t vo, addr_t pa, size_t size, FUNC &&func)
		{
			/* sanity check vo bits 38 to 63 must be equal */
			addr_t sanity = vo >> 38;
			if (sanity != 0 && sanity != 0x3ffffff) {
				Genode::error("invalid virtual address: ", vo);
				throw Invalid_range();
			}

			/* clear bits 39 - 63 */
			vo &= VM_MASK;

			for (size_t i = vo >> BLOCK_SIZE_LOG2; size > 0;
			     i = vo >> BLOCK_SIZE_LOG2) {
				addr_t end = (vo + BLOCK_SIZE) & BLOCK_MASK;
				size_t sz  = min(size, end-vo);

				func(vo, pa, sz, _entries[i]);

				/* flush cached table entry address */
				_translation_added((addr_t)&_entries[i], sz);

				/* check whether we wrap */
				if (end < vo) return;

				size = size - sz;
				vo  += sz;
				pa  += sz;
			}
		}

		template <typename E>
		struct Insert_func
		{
			Genode::Page_flags const & flags;
			Allocator            & alloc;

			Insert_func(Genode::Page_flags const & flags, Allocator & alloc)
			: flags(flags), alloc(alloc) { }

			void operator () (addr_t const          vo,
			                  addr_t const          pa,
			                  size_t const          size,
			                  typename Descriptor::access_t &desc)
			{
				using Td = Table_descriptor;

				/* can we insert a whole block? */
				if (!((vo & ~BLOCK_MASK) || (pa & ~BLOCK_MASK) || size < BLOCK_SIZE)) {
					typename Descriptor::access_t blk_desc =
						Block_descriptor::create(flags, pa);

					if (Descriptor::valid(desc) && desc != blk_desc)
						throw Double_insertion();

					desc = blk_desc;
					return;
				}

				/* we need to use a next level table */
				switch (Descriptor::type(desc)) {

				case Descriptor::INVALID: /* no entry */
					{
						/* create and link next level table */
						E & table = alloc.construct<E>();
						desc = Td::create((void*)alloc.phys_addr(table));
					}
					[[fallthrough]];

				case Descriptor::TABLE: /* table already available */
					{
						/* use allocator to retrieve virt address of table */
						E & table = alloc.virt_addr<E>(Td::Base::bits(Td::Ppn::get(desc)));
						table.insert_translation(vo - (vo & BLOCK_MASK),
						                         pa, size, flags, alloc);
						break;
					}

				case Descriptor::BLOCK: /* there is already a block */
						throw Double_insertion();
				};
			}
		};

		template <typename E>
		struct Remove_func
		{
			Allocator & alloc;

			Remove_func(Allocator & alloc) : alloc(alloc) { }

			void operator () (addr_t const    vo,
			                  addr_t const /* pa */,
			                  size_t const    size,
			                  typename Descriptor::access_t &desc)
			{
				using Td = Table_descriptor;

				switch (Descriptor::type(desc)) {
				case Descriptor::TABLE:
					{
						/* use allocator to retrieve virt address of table */
						E & table = alloc.virt_addr<E>(Td::Base::bits(Td::Ppn::get(desc)));
						table.remove_translation(vo - (vo & BLOCK_MASK), size, alloc);
						if (!table.empty()) break;
						alloc.destruct<E>(table);
					}
					[[fallthrough]];
				case Descriptor::BLOCK: [[fallthrough]];
				case Descriptor::INVALID:
					desc = 0;
				}
			}
		};

	public:

		Level_x_translation_table()
		{
			if (!_aligned((addr_t)this, ALIGNM_LOG2)) {
				Genode::warning("misaligned address");
				throw Misaligned();
			}

			memset(&_entries, 0, sizeof(_entries));
		}

		bool empty()
		{
			for (unsigned i = 0; i < MAX_ENTRIES; i++)
				if (Descriptor::valid(_entries[i]))
					return false;
			return true;
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo     offset of the virtual region represented
		 *               by the translation within the virtual
		 *               region represented by this table
		 * \param pa     base of the physical backing store
		 * \param size   size of the translated region
		 * \param flags  mapping flags
		 * \param alloc  level allocator
		 */
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Genode::Page_flags const & flags,
		                        Allocator & alloc )
		{
			_range_op(vo, pa, size, Insert_func<ENTRY>(flags, alloc));
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc level allocator
		 */
		void remove_translation(addr_t vo, size_t size, Allocator & alloc)
		{
			_range_op(vo, 0, size, Remove_func<ENTRY>(alloc));
		}

		bool lookup_rw_translation(addr_t, addr_t &, Allocator &)
		{
			Genode::raw(__func__, " not implemented yet");
			return false;
		}
}  __attribute__((aligned(1 << ALIGNM_LOG2)));


namespace Sv39 {

	/**
	 * Insert/Remove functor specialization for level 3
	 */
	template <> template <>
	struct Level_3_translation_table::Insert_func<None>
	{
		Genode::Page_flags const & flags;

		Insert_func(Genode::Page_flags const & flags, Allocator &)
		: flags(flags) { }

		void operator () (addr_t const          vo,
		                  addr_t const          pa,
		                  size_t const          size,
		                  Descriptor::access_t &desc)
		{
			if ((vo & ~BLOCK_MASK) || (pa & ~BLOCK_MASK) ||
			    size < BLOCK_SIZE) {
				Genode::warning("invalid range");
				throw Invalid_range();
			}

			Descriptor::access_t blk_desc =
				Block_descriptor::create(flags, pa);

			if (Descriptor::valid(desc) && desc != blk_desc)
				throw Double_insertion();

			desc = blk_desc;
		}
	};

	template <> template <>
	struct Level_3_translation_table::Remove_func<None>
	{
		Remove_func(Allocator &) { }

		void operator () (addr_t /* vo */,
		                  addr_t /* pa */,
		                  size_t /* size */,
		                  Descriptor::access_t &desc) {
			desc = 0; }
	};
}


namespace Hw {

	struct Page_table : Sv39::Level_1_translation_table
	{
		enum {
			TABLE_LEVEL_X_SIZE_LOG2 = Sv39::SIZE_LOG2_4K,
			CORE_VM_AREA_SIZE = 512 * 1024 * 1024,
			CORE_TRANS_TABLE_COUNT =
				_count(CORE_VM_AREA_SIZE, Sv39::SIZE_LOG2_1G) +
				_count(CORE_VM_AREA_SIZE, Sv39::SIZE_LOG2_2M),
		};

		Page_table() : Sv39::Level_1_translation_table() {}

		Page_table(Page_table & kernel_table)
		: Sv39::Level_1_translation_table() {
			static unsigned first = (0xffffffc000000000UL & VM_MASK) >> Sv39::SIZE_LOG2_1G;
			for (unsigned i = first; i < MAX_ENTRIES; i++)
				_entries[i] = kernel_table._entries[i]; }
	};
}

#endif /* _SRC__LIB__HW__SPEC__RISCV__PAGE_TABLE_H_ */
