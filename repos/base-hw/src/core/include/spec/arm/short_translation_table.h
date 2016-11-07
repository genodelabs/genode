/*
 * \brief   Short descriptor translation table definitions
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM__SHORT_TRANSLATION_TABLE_H_
#define _CORE__INCLUDE__SPEC__ARM__SHORT_TRANSLATION_TABLE_H_

/* Genode includes */
#include <util/register.h>

/* core includes */
#include <util.h>
#include <assert.h>
#include <page_flags.h>
#include <translation_table_allocator.h>
#include <cpu.h>

namespace Genode
{
	/**
	 * Commons for all translations
	 */
	class Translation;

	/**
	 * Second level translation table
	 */
	class Page_table;

	/**
	 * First level translation table
	 */
	class Translation_table;
}

class Genode::Translation
{
	private:

		/**
		 * Return TEX value for device-memory translations
		 */
		static constexpr unsigned _device_tex();

	protected:

		/**
		 * Return translation according to flags 'f' and phys. address 'pa'
		 */
		template <typename T> static typename T::access_t
		_create(Page_flags const & f, addr_t const pa)
		{
			typename T::access_t v = T::Pa::masked(pa);
			T::S::set(v, Kernel::board().is_smp());
			T::Ng::set(v, !f.global);
			T::Xn::set(v, !f.executable);
			if (f.device) { T::Tex::set(v, _device_tex()); }
			else {
			switch (f.cacheable) {
			case         CACHED: T::Tex::set(v, 5);
			case WRITE_COMBINED: T::B::set(v, 1);   break;
			case       UNCACHED: T::Tex::set(v, 1); break; } }
			if (f.writeable) if (f.privileged) T::Ap::set(v, 1);
				             else              T::Ap::set(v, 3);
			else             if (f.privileged) T::Ap::set(v, 5);
				             else              T::Ap::set(v, 2);
			return v;
		}
};

class Genode::Page_table
{
	public:

		enum {
			SIZE_LOG2   = 10,
			SIZE        = 1 << SIZE_LOG2,
			ALIGNM_LOG2 = SIZE_LOG2,
		};

		/**
		 * Common descriptor structure
		 */
		struct Descriptor : Register<32>, Translation
		{
			enum Type { FAULT, SMALL_PAGE };

			enum {
				VIRT_SIZE_LOG2   = 12,
				VIRT_SIZE        = 1 << VIRT_SIZE_LOG2,
				VIRT_OFFSET_MASK = (1 << VIRT_SIZE_LOG2) - 1,
				VIRT_BASE_MASK   = ~(VIRT_OFFSET_MASK),
			};

			struct Type_0 : Bitfield<0, 2> { };
			struct Type_1 : Bitfield<1, 1> { };

			/**
			 * Get descriptor type of 'v'
			 */
			static Type type(access_t const v)
			{
				access_t const t0 = Type_0::get(v);
				if (t0 == 0) { return FAULT; }
				access_t const t1 = Type_1::get(v);
				if (t1 == 1) { return SMALL_PAGE; }
				return FAULT;
			}

			/**
			 * At descriptor value 'v' set type to 't'
			 */
			static void type(access_t & v, Type const t)
			{
				switch (t) {
				case FAULT:      Type_0::set(v, 0); return;
				case SMALL_PAGE: Type_1::set(v, 1); return; }
			}

			static void invalidate(access_t & v) { type(v, FAULT); }

			static bool valid(access_t & v) { return type(v) != FAULT; }
		};

		/**
		 * Small page descriptor structure
		 */
		struct Small_page : Descriptor
		{
			struct Xn   : Bitfield<0, 1> { };       /* execute never */
			struct B    : Bitfield<2, 1> { };       /* mem region attr. */
			struct Ap_0 : Bitfield<4, 2> { };       /* access permission */
			struct Tex  : Bitfield<6, 3> { };       /* mem region attr. */
			struct Ap_1 : Bitfield<9, 1> { };       /* access permission */
			struct S    : Bitfield<10, 1> { };      /* shareable bit */
			struct Ng   : Bitfield<11, 1> { };      /* not global bit */
			struct Pa   : Bitfield<12, 20> { };     /* physical base */
			struct Ap   : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

			/**
			 * Return page descriptor for physical address 'pa' and 'flags'
			 */
			static access_t create(Page_flags const & flags, addr_t const pa)
			{
				access_t v = _create<Small_page>(flags, pa);
				Descriptor::type(v, Descriptor::SMALL_PAGE);
				return v;
			}
		};

	private:

		/* table payload that must be the only member of this class */
		Descriptor::access_t _entries[SIZE/sizeof(Descriptor::access_t)];

		enum { MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1 };

		/**
		 * Get entry index by virtual offset
		 *
		 * \param i   is overridden with the index if call returns 0
		 * \param vo  virtual offset relative to the virtual table base
		 *
		 * \retval  0  on success
		 * \retval <0  translation failed
		 */
		bool _index_by_vo (unsigned & i, addr_t const vo) const
		{
			if (vo > max_virt_offset()) return false;
			i = vo >> Descriptor::VIRT_SIZE_LOG2;
			return true;
		}

	public:

		/**
		 * Constructor
		 */
		Page_table()
		{
			assert(aligned(this, ALIGNM_LOG2));
			memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Maximum virtual offset that can be translated by this table
		 */
		static addr_t max_virt_offset()
		{
			return (MAX_INDEX << Descriptor::VIRT_SIZE_LOG2)
			        + (Descriptor::VIRT_SIZE - 1);
		}

		/**
		 * Insert one atomic translation into this table
		 *
		 * \param vo    offset of the virtual region represented
		 *              by the translation within the virtual
		 *              region represented by this table
		 * \param pa    base of the physical backing store
		 * \param size  size of the translated region
		 * \param flags mapping flags
		 */
		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags)
		{
			constexpr size_t sz = Descriptor::VIRT_SIZE;

			for (unsigned i; (size > 0) && _index_by_vo(i, vo);
			     size = (size < sz) ? 0 : size - sz,
			     vo += sz, pa += sz)
			{

				/* compose new descriptor value */
				Small_page::access_t const e =
					Small_page::create(flags, pa);

				/* check if it is a good idea to override the entry */
				assert(!Descriptor::valid(_entries[i]) ||
				       _entries[i] == e);

				/* override entry */
				_entries[i] = e;

				/* some CPUs need to act on changed translations */
				Cpu::translation_added((addr_t)&_entries[i],
				                       sizeof(Descriptor::access_t));
			}
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 */
		void remove_translation(addr_t vo, size_t size)
		{
			constexpr size_t sz = Descriptor::VIRT_SIZE;

			for (unsigned i; (size > 0) && _index_by_vo(i, vo);
			     size = (size < sz) ? 0 : size - sz, vo += sz)
			{
				switch (Descriptor::type(_entries[i])) {
				case Descriptor::SMALL_PAGE:
					Descriptor::invalidate(_entries[i]);
				default: ;
				}
			}
		}

		/**
		 * Does this table solely contain invalid entries
		 */
		bool empty()
		{
			for (unsigned i = 0; i <= MAX_INDEX; i++) {
				if (Descriptor::valid(_entries[i])) return false; }
			return true;
		}

} __attribute__((aligned(1<<Page_table::ALIGNM_LOG2)));

class Genode::Translation_table
{
	public:

		enum {
			SIZE_LOG2               = 14,
			SIZE                    = 1 << SIZE_LOG2,
			ALIGNM_LOG2             = SIZE_LOG2,
			MAX_PAGE_SIZE_LOG2      = 20,
			MIN_PAGE_SIZE_LOG2      = 12,
			TABLE_LEVEL_X_VIRT_SIZE = 1 << MAX_PAGE_SIZE_LOG2,
			TABLE_LEVEL_X_SIZE_LOG2 = MIN_PAGE_SIZE_LOG2,
			CORE_VM_AREA_SIZE       = 1024 * 1024 * 1024,
			CORE_TRANS_TABLE_COUNT  = CORE_VM_AREA_SIZE / TABLE_LEVEL_X_VIRT_SIZE,
		};

		/**
		 * A first level translation descriptor
		 */
		struct Descriptor : Register<32>, Translation
		{
			enum Type { FAULT, PAGE_TABLE, SECTION };

			enum {
				VIRT_SIZE_LOG2   = 20,
				VIRT_SIZE        = 1 << VIRT_SIZE_LOG2,
				VIRT_OFFSET_MASK = (1 << VIRT_SIZE_LOG2) - 1,
				VIRT_BASE_MASK   = ~VIRT_OFFSET_MASK,
			};

			struct Type_0   : Bitfield<0, 2> { };
			struct Type_1_0 : Bitfield<1, 1> { };
			struct Type_1_1 : Bitfield<18, 1> { };
			struct Type_1   : Bitset_2<Type_1_0, Type_1_1> { };

			/**
			 * Get descriptor type of 'v'
			 */
			static Type type(access_t const v)
			{
				switch (Type_0::get(v)) {
				case 0: return FAULT;
				case 1: return PAGE_TABLE; }

				if (Type_1::get(v) == 1) { return SECTION; }
				return FAULT;
			}

			/**
			 * Set descriptor type of 'v'
			 */
			static void type(access_t & v, Type const t)
			{
				switch (t) {
				case FAULT:      Type_0::set(v, 0); return;
				case PAGE_TABLE: Type_0::set(v, 1); return;
				case SECTION:    Type_1::set(v, 1); return; }
			}

			static void invalidate(access_t & v) { type(v, FAULT); }

			static bool valid(access_t & v) { return type(v) != FAULT; }

			static inline Type align(addr_t vo, addr_t pa, size_t size)
			{
				return ((vo & VIRT_OFFSET_MASK) || (pa & VIRT_OFFSET_MASK) ||
				        size < VIRT_SIZE) ? PAGE_TABLE : SECTION;
			}
		};

		/**
		 * Link to a second level translation table
		 */
		struct Page_table_descriptor : Descriptor
		{
			struct Domain : Bitfield<5, 4> { };   /* domain        */
			struct Pa     : Bitfield<10, 22> { }; /* physical base */

			/**
			 * Return descriptor value for page table 'pt
			 */
			static access_t create(Page_table * const pt)
			{
				access_t v = Pa::masked((addr_t)pt);
				Descriptor::type(v, Descriptor::PAGE_TABLE);
				return v;
			}
		};

		/**
		 * Section translation descriptor
		 */
		struct Section : Descriptor
		{
			struct B    : Bitfield<2, 1> { };       /* mem. region attr. */
			struct Xn   : Bitfield<4, 1> { };       /* execute never bit */
			struct Ap_0 : Bitfield<10, 2> { };      /* access permission */
			struct Tex  : Bitfield<12, 3> { };      /* mem. region attr. */
			struct Ap_1 : Bitfield<15, 1> { };      /* access permission */
			struct S    : Bitfield<16, 1> { };      /* shared            */
			struct Ng   : Bitfield<17, 1> { };      /* not global        */
			struct Pa   : Bitfield<20, 12> { };     /* physical base     */
			struct Ap   : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

			/**
			 * Return section descriptor for physical address 'pa' and 'flags'
			 */
			static access_t create(Page_flags const & flags, addr_t const pa)
			{
				access_t v = _create<Section>(flags, pa);
				Descriptor::type(v, Descriptor::SECTION);
				return v;
			}
		};

	protected:

		/* table payload, must be the first member of this class */
		Descriptor::access_t _entries[SIZE/sizeof(Descriptor::access_t)];

		enum { MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1 };

		/**
		 * Try to get entry index in 'i' for virtual offset 'vo!
		 *
		 * \return  whether it was successful
		 */
		bool _index_by_vo(unsigned & i, addr_t const vo) const
		{
			if (vo > max_virt_offset()) return false;
			i = vo >> Descriptor::VIRT_SIZE_LOG2;
			return true;
		}

		/**
		 * Insert a second level translation at the given entry index
		 *
		 * \param i     the related index
		 * \param vo    virtual start address of range
		 * \param pa    physical start address of range
		 * \param size  size of range
		 * \param flags mapping flags
		 * \param alloc second level translation table allocator
		 */
		void _insert_second_level(unsigned i, addr_t const vo, addr_t const pa,
		                          size_t const size, Page_flags const & flags,
		                          Translation_table_allocator * const alloc)
		{
			Page_table * pt = 0;
			switch (Descriptor::type(_entries[i])) {

			case Descriptor::FAULT:
				{
					if (!alloc) throw Allocator::Out_of_memory();

					/* create and link page table */
					pt = new (alloc) Page_table();
					Page_table * pt_phys = (Page_table*) alloc->phys_addr(pt);
					pt_phys = pt_phys ? pt_phys : pt; /* hack for core */
					_entries[i] = Page_table_descriptor::create(pt_phys);

					/* some CPUs need to act on changed translations */
					size_t const dsize = sizeof(Descriptor::access_t);
					Cpu::translation_added((addr_t)&_entries[i], dsize);
				}

			case Descriptor::PAGE_TABLE:
				{
					/* use allocator to retrieve virtual addr. of page table */
					void * pt_phys = (void*)
						Page_table_descriptor::Pa::masked(_entries[i]);
					pt = (Page_table *) alloc->virt_addr(pt_phys);
					pt = pt ? pt : (Page_table *)pt_phys ; /* hack for core */
					break;
				}

			default: assert(0);
			}

			/* insert translation */
			addr_t const pt_vo = vo - Section::Pa::masked(vo);
			pt->insert_translation(pt_vo, pa, size, flags);
		}

	public:

		/**
		 * Constructor
		 */
		Translation_table()
		{
			assert(aligned(this, ALIGNM_LOG2));
			memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * Maximum virtual offset that can be translated by this table
		 */
		static addr_t max_virt_offset()
		{
			constexpr addr_t base = MAX_INDEX << Descriptor::VIRT_SIZE_LOG2;
			return base + (Descriptor::VIRT_SIZE - 1);
		}

		/**
		 * Insert translations into this table
		 *
		 * \param vo    offset of virt. transl. region in virt. table region
		 * \param pa    base of physical backing store
		 * \param size  size of translated region
		 * \param f     mapping flags
		 * \param alloc second level translation table allocator
		 */
		void insert_translation(addr_t vo, addr_t pa, size_t size,
		                        Page_flags const & f,
		                        Translation_table_allocator * const alloc)
		{
			/* check sanity */
			assert(!(vo & Page_table::Descriptor::VIRT_OFFSET_MASK) &&
			       size >= Page_table::Descriptor::VIRT_SIZE);

			for (unsigned i; (size > 0) && _index_by_vo (i, vo);) {

				addr_t const ve = (vo + Descriptor::VIRT_SIZE);
				addr_t const end = ve & Descriptor::VIRT_BASE_MASK;

				/* decide granularity of entry that can be inserted */
				switch (Descriptor::align(vo, pa, size)) {

				case Descriptor::SECTION:
					{
						/* compose new entry */
						Section::access_t const e = Section::create(f, pa);

						/* override entry */
						if (_entries[i] == e) { break; }
						assert(!Descriptor::valid(_entries[i]));
						_entries[i] = e;

						/* some CPUs need to act on changed translations */
						constexpr size_t ds = sizeof(Descriptor::access_t);
						Cpu::translation_added((addr_t)&_entries[i], ds);
						break;
					}

				default:
					_insert_second_level(i, vo, pa, min(size, end - vo), f,
					                     alloc);
				};

				/* check whether we wrap */
				if (end < vo) { return; }

				size_t sz  = end - vo;
				size = (size > sz) ? size - sz : 0;
				vo += sz;
				pa += sz;
			}
		}

		/**
		 * Remove translations that overlap with a given virtual region
		 *
		 * \param vo    region offset within the tables virtual region
		 * \param size  region size
		 * \param alloc second level translation table allocator
		 */
		void remove_translation(addr_t vo, size_t size,
		                        Translation_table_allocator * alloc)
		{
			/* check sanity */
			assert(vo <= (vo + size));

			for (unsigned i; (size > 0) && _index_by_vo(i, vo);) {

				constexpr addr_t dbm = Descriptor::VIRT_BASE_MASK;
				addr_t const end = (vo + Descriptor::VIRT_SIZE) & dbm;

				switch (Descriptor::type(_entries[i])) {

				case Descriptor::PAGE_TABLE:
					{
						typedef Page_table_descriptor Ptd;
						typedef Page_table            Pt;

						Pt * pt_phys = (Pt *) Ptd::Pa::masked(_entries[i]);
						Pt * pt      = (Pt *) alloc->virt_addr(pt_phys);
						pt = pt ? pt : pt_phys; // TODO hack for core

						addr_t const pt_vo = vo - Section::Pa::masked(vo);
						pt->remove_translation(pt_vo, min(size, end-vo));

						if (pt->empty()) {
							Descriptor::invalidate(_entries[i]);
							destroy(alloc, pt);
						}
						break;
					}

				default: Descriptor::invalidate(_entries[i]); }

				/* check whether we wrap */
				if (end < vo) return;

				size_t sz  = end - vo;
				size = (size > sz) ? size - sz : 0;
				vo += sz;
			}
		}
} __attribute__((aligned(1<<Translation_table::ALIGNM_LOG2)));

#endif /* _CORE__INCLUDE__SPEC__ARM__SHORT_TRANSLATION_TABLE_H_ */
