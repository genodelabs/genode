/*
 * \brief   Standard ARM v7 2-level page table format
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_
#define _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_

#include <cpu/page_flags.h>
#include <hw/util.h>
#include <hw/assert.h>
#include <hw/page_table_allocator.h>
#include <util/register.h>

namespace Hw {
	using namespace Genode;

	class Page_table;
}


class Hw::Page_table
{
	private:

		template <unsigned long B>
		using Register = Genode::Register<B>;

		template <typename T, typename U>
		using Bitset_2 = Genode::Bitset_2<T,U>;

		class Descriptor_base
		{
			protected:

				/**
				 * Return TEX value for device-memory
				 */
				static constexpr unsigned _device_tex();

				/**
				 * Return whether this is a SMP system
				 */
				static constexpr bool     _smp();

				/**
				 * Return descriptor value according to physical address 'pa'
				 */
				template <typename T>
				static typename T::access_t _create(Page_flags const & f,
				                                    addr_t pa)
				{
					using namespace Genode;

					typename T::access_t v = T::Pa::masked(pa);
					T::S::set(v, _smp());
					T::Ng::set(v, !f.global);
					T::Xn::set(v, !f.executable);
					if (f.type == DEVICE) {
						T::Tex::set(v, _device_tex());
					} else {
						switch (f.cacheable) {
							case         CACHED: T::Tex::set(v, 5); [[fallthrough]];
							case WRITE_COMBINED: T::B::set(v, 1);   break;
							case       UNCACHED: T::Tex::set(v, 1); break;
						}
					}
					if (f.writeable) if (f.privileged) T::Ap::set(v, 1);
					else              T::Ap::set(v, 3);
					else             if (f.privileged) T::Ap::set(v, 5);
					else              T::Ap::set(v, 2);
					return v;
				}
		};

		class Page_table_level_2
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
				struct Descriptor : Register<32>, Descriptor_base
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

					static bool valid(access_t & v) {
						return type(v) != FAULT; }
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
					static access_t create(Page_flags const & flags,
							addr_t const pa)
					{
						access_t v = _create<Small_page>(flags, pa);
						Descriptor::type(v, Descriptor::SMALL_PAGE);
						return v;
					}
				};

			private:

				static constexpr size_t cnt =
					SIZE / sizeof(Descriptor::access_t);

				Descriptor::access_t _entries[cnt];

				enum {
					MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1
				};

				/**
				 * Get entry index by virtual offset
				 *
				 * \param i   is overridden with the index if call returns 0
				 * \param vo  virtual offset relative to the virtual table base
				 *
				 * \retval  0  on success
				 * \retval <0  translation failed
				 */
				bool _index_by_vo(unsigned & i, addr_t const vo) const
				{
					if (vo > max_virt_offset()) return false;
					i = vo >> Descriptor::VIRT_SIZE_LOG2;
					return true;
				}

			public:

				Page_table_level_2()
				{
					assert(aligned(this, ALIGNM_LOG2));
					Genode::memset(&_entries, 0, sizeof(_entries));
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
				 * Lookup writeable translation
				 *
				 * \param virt  virtual address offset to look for
				 * \param phys  physical address to return
				 * \returns true if lookup was successful, otherwise false
				 */
				bool lookup_rw_translation(addr_t const virt, addr_t & phys)
				{
					unsigned idx = 0;
					if (!_index_by_vo(idx, virt)) return false;

					switch (Descriptor::type(_entries[idx])) {
					case Descriptor::SMALL_PAGE:
						{
							Small_page::access_t ap =
								Small_page::Ap::get(_entries[idx]);
							phys = Small_page::Pa::masked(_entries[idx]);
							return ap == 1 || ap == 3;
						}
					default: return false;
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

		} __attribute__((aligned(1<<Page_table_level_2::ALIGNM_LOG2)));


	public:

		using Allocator = Hw::Page_table_allocator<Page_table_level_2::SIZE>;

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
		struct Descriptor : Register<32>, Descriptor_base
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
			static access_t create(addr_t pt)
			{
				access_t v = Pa::masked(pt);
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

		static inline void _table_changed(addr_t, size_t);

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
		                          Allocator & alloc)
		{
			if (i > MAX_INDEX)
				return;

			using Pt  = Page_table_level_2;
			using Ptd = Page_table_descriptor;

			switch (Descriptor::type(_entries[i])) {

			case Descriptor::FAULT:
				{
					/* create and link page table */
					Pt & pt = alloc.construct<Pt>();
					_entries[i] = Ptd::create(alloc.phys_addr(pt));
					_table_changed((addr_t)&_entries[i], sizeof(Ptd));
				}
				[[fallthrough]];

			case Descriptor::PAGE_TABLE:
				{
					Pt & pt = alloc.virt_addr<Pt>(Ptd::Pa::masked(_entries[i]));
					pt.insert_translation(vo - Section::Pa::masked(vo), pa, size, flags);
					_table_changed((addr_t)&pt, sizeof(Page_table_level_2));
					break;
				}

			default: assert(0);
			}

		}

	public:

		Page_table()
		{
			assert(aligned(this, ALIGNM_LOG2));
			Genode::memset(&_entries, 0, sizeof(_entries));
		}

		/**
		 * On ARM we do not need to copy top-level kernel entries
		 * because the virtual-memory kernel part is hold in a separate table
		 */
		explicit Page_table(Page_table &) : Page_table() { }

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
		                        Page_flags const & f, Allocator & alloc)
		{
			/* check sanity */
			assert(!(vo & Page_table_level_2::Descriptor::VIRT_OFFSET_MASK) &&
			       size >= Page_table_level_2::Descriptor::VIRT_SIZE);

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
						_table_changed((addr_t)&_entries[i], sizeof(Section));
						break;
					}

				default:
					_insert_second_level(i, vo, pa, Genode::min(size, end - vo), f,
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
		void remove_translation(addr_t vo, size_t size, Allocator & alloc)
		{
			using Pt = Page_table_level_2;
			/* check sanity */
			assert(vo <= (vo + size));

			for (unsigned i; (size > 0) && _index_by_vo(i, vo);) {

				constexpr addr_t dbm = Descriptor::VIRT_BASE_MASK;
				addr_t const end = (vo + Descriptor::VIRT_SIZE) & dbm;

				switch (Descriptor::type(_entries[i])) {

				case Descriptor::PAGE_TABLE:
					{
						using Ptd = Page_table_descriptor;

						Pt & pt =
							alloc.virt_addr<Pt>(Ptd::Pa::masked(_entries[i]));

						addr_t const pt_vo = vo - Section::Pa::masked(vo);
						pt.remove_translation(pt_vo, Genode::min(size, end-vo));
						_table_changed((addr_t)&pt, sizeof(Page_table_level_2));

						if (pt.empty()) {
							Descriptor::invalidate(_entries[i]);
							alloc.destruct<Pt>(pt);
						}
						break;
					}

				default:
					{
						Descriptor::invalidate(_entries[i]);
						_table_changed((addr_t)&_entries[i], sizeof(Section));
					}
				}

				/* check whether we wrap */
				if (end < vo) return;

				size_t sz  = end - vo;
				size = (size > sz) ? size - sz : 0;
				vo += sz;
			}
		}

		/**
		 * Lookup writeable translation
		 *
		 * \param virt  virtual address to look at
		 * \param phys  physical address to return
		 * \param alloc second level translation table allocator
		 * \returns true if a translation was found, otherwise false
		 */
		bool lookup_rw_translation(addr_t const virt, addr_t & phys,
		                           Allocator & alloc)
		{
			unsigned idx = 0;
			if (!_index_by_vo(idx, virt)) return false;

			switch (Descriptor::type(_entries[idx])) {

			case Descriptor::SECTION:
				{
					Section::access_t ap =
						Section::Ap::get(_entries[idx]);
					phys = Section::Pa::masked(_entries[idx]);
					return ap == 1 || ap == 3;
				}

			case Descriptor::PAGE_TABLE:
				{
					using Pt  = Page_table_level_2;
					using Ptd = Page_table_descriptor;

					Pt & pt =
						alloc.virt_addr<Pt>(Ptd::Pa::masked(_entries[idx]));

					addr_t const offset = virt - Section::Pa::masked(virt);
					return pt.lookup_rw_translation(offset, phys);
				}
			default: return false;
			};
		}
} __attribute__((aligned(1<<Page_table::ALIGNM_LOG2)));

#endif /* _SRC__LIB__HW__SPEC__ARM__PAGE_TABLE_H_ */
