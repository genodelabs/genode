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

#ifndef _ARM__SHORT_TRANSLATION_TABLE_H_
#define _ARM__SHORT_TRANSLATION_TABLE_H_

/* Genode includes */
#include <util/register.h>
#include <base/printf.h>

/* base-hw includes */
#include <page_flags.h>
#include <page_slab.h>

namespace Arm
{
	using namespace Genode;

	/**
	 * Check if 'p' is aligned to 1 << 'alignm_log2'
	 */
	inline bool aligned(addr_t const a, size_t const alignm_log2) {
		return a == ((a >> alignm_log2) << alignm_log2); }

	/**
	 * Return permission configuration according to given mapping flags
	 *
	 * \param T      targeted translation-table-descriptor type
	 * \param flags  mapping flags
	 *
	 * \return  descriptor value with AP and XN set and the rest left zero
	 */
	template <typename T>
	static typename T::access_t
	access_permission_bits(Page_flags const & flags)
	{
		typedef typename T::Xn Xn;
		typedef typename T::Ap Ap;
		typedef typename T::access_t access_t;
		bool const w = flags.writeable;
		bool const p = flags.privileged;
		access_t ap;
		if (w) { if (p) { ap = Ap::bits(0b001); }
		         else   { ap = Ap::bits(0b011); }
		} else { if (p) { ap = Ap::bits(0b101); }
		         else   { ap = Ap::bits(0b010); }
		}
		return Xn::bits(!flags.executable) | ap;
	}

	/**
	 * Memory region attributes for the translation descriptor 'T'
	 */
	template <typename T>
	static typename T::access_t
	memory_region_attr(Page_flags const & flags);

	class Section_table;
}

/**
 * First level translation table
 */
class Arm::Section_table
{
	public:

		/***************************
		 ** Exception definitions **
		 ***************************/

		class Double_insertion {};
		class Misaligned {};
		class Invalid_range {};

	private:

		/**
		 * Second level translation table
		 */
		class Page_table
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
				struct Descriptor : Register<32>
				{
					/**
					 * Descriptor types
					 */
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
						if (t1 == 1) return SMALL_PAGE;
						return FAULT;
					}

					/**
					 * Set descriptor type of 'v'
					 */
					static void type(access_t & v, Type const t)
					{
						switch (t) {
						case FAULT:
							Type_0::set(v, 0);
							return;
						case SMALL_PAGE:
							Type_1::set(v, 1);
							return;
						}
					}

					/**
					 * Invalidate descriptor 'v'
					 */
					static void invalidate(access_t & v) { type(v, FAULT); }

					/**
					 * Return if descriptor 'v' is valid
					 */
					static bool valid(access_t & v) {
						return type(v) != FAULT; }
				};

				/**
				 * Small page descriptor structure
				 */
				struct Small_page : Descriptor
				{
					struct Xn   : Bitfield<0, 1> { };   /* execute never */
					struct B    : Bitfield<2, 1> { };   /* mem region attr. */
					struct C    : Bitfield<3, 1> { };   /* mem region attr. */
					struct Ap_0 : Bitfield<4, 2> { };   /* access permission */
					struct Tex  : Bitfield<6, 3> { };   /* mem region attr. */
					struct Ap_1 : Bitfield<9, 1> { };   /* access permission */
					struct S    : Bitfield<10, 1> { };  /* shareable bit */
					struct Ng   : Bitfield<11, 1> { };  /* not global bit */
					struct Pa   : Bitfield<12, 20> { }; /* physical base */

					struct Ap : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

					/**
					 * Compose descriptor value
					 */
					static access_t create(Page_flags const & flags,
					                       addr_t const pa)
					{
						access_t v = access_permission_bits<Small_page>(flags);
						v |= memory_region_attr<Small_page>(flags);
						v |= Ng::bits(!flags.global);
						v |= S::bits(1);
						v |= Pa::masked(pa);
						Descriptor::type(v, Descriptor::SMALL_PAGE);
						return v;
					}
				};

			private:

				/*
				 * Table payload
				 *
				 * Must be the only member of this class
				 */
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
					if (!aligned((addr_t)this, ALIGNM_LOG2))
						throw Misaligned();

					/* start with an empty table */
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
				 *
				 * This method overrides an existing translation in case
				 * that it spans the the same virtual range otherwise it
				 * throws a Double_insertion.
				 */
				void insert_translation(addr_t vo,
				                        addr_t pa,
				                        size_t size,
				                        Page_flags const & flags)
				{
					constexpr size_t sz = Descriptor::VIRT_SIZE;

					for (unsigned i; (size > 0) && _index_by_vo (i, vo);
						 size = (size < sz) ? 0 : size - sz, vo += sz, pa += sz) {

						if (Descriptor::valid(_entries[i]) &&
							_entries[i] != Small_page::create(flags, pa))
							throw Double_insertion();

						/* compose new descriptor value */
						_entries[i] = Small_page::create(flags, pa);
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
					     size = (size < sz) ? 0 : size - sz, vo += sz) {

						switch (Descriptor::type(_entries[i])) {

						case Descriptor::SMALL_PAGE:
							{
								Descriptor::invalidate(_entries[i]);
							}

						default: ;
						}
					}
				}

				/**
				 * Does this table solely contain invalid entries
				 */
				bool empty()
				{
					for (unsigned i = 0; i <= MAX_INDEX; i++)
						if (Descriptor::valid(_entries[i])) return false;
					return true;
				}

		} __attribute__((aligned(1<<Page_table::ALIGNM_LOG2)));


		enum { DOMAIN = 0 };

	public:

		enum {
			SIZE_LOG2   = 14,
			SIZE        = 1 << SIZE_LOG2,
			ALIGNM_LOG2 = SIZE_LOG2,

			MAX_COSTS_PER_TRANSLATION = sizeof(Page_table),

			MAX_PAGE_SIZE_LOG2 = 20,
			MIN_PAGE_SIZE_LOG2 = 12,
		};

		/**
		 * A first level translation descriptor
		 */
		struct Descriptor : Register<32>
		{
			/**
			 * Descriptor types
			 */
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
				case 1:	return PAGE_TABLE;
				}

				switch (Type_1::get(v)) {
				case 1: return SECTION;
				}

				return FAULT;
			}

			/**
			 * Set descriptor type of 'v'
			 */
			static void type(access_t & v, Type const t)
			{
				switch (t) {
				case FAULT:
					Type_0::set(v, 0);
					return;
				case PAGE_TABLE:
					Type_0::set(v, 1);
					return;
				case SECTION:
					Type_1::set(v, 1);
					return;
				}
			}

			/**
			 * Invalidate descriptor 'v'
			 */
			static void invalidate(access_t & v) { type(v, FAULT); }

			/**
			 * Return if descriptor 'v' is valid
			 */
			static bool valid(access_t & v) { return type(v) != FAULT; }

			static inline Type align(addr_t vo, size_t size)
			{
				return ((vo & VIRT_OFFSET_MASK) || size < VIRT_SIZE)
				       ? PAGE_TABLE : SECTION;
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
			 * Compose descriptor value
			 */
			static access_t create(Page_table * const pt)
			{
					access_t v = Domain::bits(DOMAIN) |
					             Pa::masked((addr_t)pt);
					Descriptor::type(v, Descriptor::PAGE_TABLE);
					return v;
			}
		};

		/**
		 * Section translation descriptor
		 */
		struct Section : Descriptor
		{
			struct B      : Bitfield<2, 1> { };       /* mem. region attr. */
			struct C      : Bitfield<3, 1> { };       /* mem. region attr. */
			struct Xn     : Bitfield<4, 1> { };       /* execute never bit */
			struct Domain : Bitfield<5, 4> { };       /* domain            */
			struct Ap_0   : Bitfield<10, 2> { };      /* access permission */
			struct Tex    : Bitfield<12, 3> { };      /* mem. region attr. */
			struct Ap_1   : Bitfield<15, 1> { };      /* access permission */
			struct S      : Bitfield<16, 1> { };      /* shared            */
			struct Ng     : Bitfield<17, 1> { };      /* not global        */
			struct Pa     : Bitfield<20, 12> { };     /* physical base     */
			struct Ap     : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

			/**
			 * Compose descriptor value
			 */
			static access_t create(Page_flags const & flags,
			                       addr_t const pa)
			{
				access_t v = access_permission_bits<Section>(flags);
				v |= memory_region_attr<Section>(flags);
				v |= Domain::bits(DOMAIN);
				v |= S::bits(1);
				v |= Ng::bits(!flags.global);
				v |= Pa::masked(pa);
				Descriptor::type(v, Descriptor::SECTION);
				return v;
			}
		};

	protected:

		/* table payload, must be the first member of this class */
		Descriptor::access_t _entries[SIZE/sizeof(Descriptor::access_t)];

		enum { MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1 };

		/**
		 * Get entry index by virtual offset
		 *
		 * \param i    is overridden with the resulting index
		 * \param vo   offset within the virtual region represented
		 *             by this table
		 *
		 * \retval  0  on success
		 * \retval <0  if virtual offset couldn't be resolved,
		 *             in this case 'i' reside invalid
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
		 * \param slab  second level page slab allocator
		 */
		void _insert_second_level(unsigned i,
		                          addr_t const vo,
		                          addr_t const pa,
		                          size_t const size,
		                          Page_flags const & flags,
		                          Page_slab * slab)
		{
			Page_table * pt = 0;
			switch (Descriptor::type(_entries[i])) {

			case Descriptor::FAULT:
				{
					if (!slab) throw Allocator::Out_of_memory();

					/* create and link page table */
					pt = new (slab) Page_table();
					Page_table * pt_phys = (Page_table*) slab->phys_addr(pt);
					pt_phys = pt_phys ? pt_phys : pt; /* hack for core */
					_entries[i] = Page_table_descriptor::create(pt_phys);
				}

			case Descriptor::PAGE_TABLE:
				{
					/* use allocator to retrieve virtual address of page table */
					void * pt_phys = (void*)
						Page_table_descriptor::Pa::masked(_entries[i]);
					pt = (Page_table *) slab->virt_addr(pt_phys);
					pt = pt ? pt : (Page_table *)pt_phys ; /* hack for core */
					break;
				}

			default:
				{
					throw Double_insertion();
				}
			};

			/* insert translation */
			pt->insert_translation(vo - Section::Pa::masked(vo),
			                       pa, size, flags);
		}

	public:

		/**
		 * Placement new
		 */
		void * operator new (size_t, void * p) { return p; }

		/**
		 * Constructor
		 */
		Section_table()
		{
			if (!aligned((addr_t)this, ALIGNM_LOG2))
				throw Misaligned();

			/* start with an empty table */
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
		 * Insert translations into this table
		 *
		 * \param vo           offset of the virtual region represented
		 *                     by the translation within the virtual
		 *                     region represented by this table
		 * \param pa           base of the physical backing store
		 * \param size         size of the translated region
		 * \param flags        mapping flags
		 * \param slab  second level page slab allocator
		 */
		void insert_translation(addr_t vo,
		                        addr_t pa,
		                        size_t size,
		                        Page_flags const & flags,
		                        Page_slab * slab)
		{
			/* sanity check  */
			if ((vo & Page_table::Descriptor::VIRT_OFFSET_MASK)
				|| size < Page_table::Descriptor::VIRT_SIZE)
				throw Invalid_range();

			for (unsigned i; (size > 0) && _index_by_vo (i, vo);) {

				addr_t end = (vo + Descriptor::VIRT_SIZE)
				             & Descriptor::VIRT_BASE_MASK;

				/* decide granularity of entry that can be inserted */
				switch (Descriptor::align(vo, size)) {

				case Descriptor::SECTION:
					{
						if (Descriptor::valid(_entries[i]) &&
						    _entries[i] != Section::create(flags, pa))
							throw Double_insertion();
						_entries[i] = Section::create(flags, pa);
						break;
					}

				default:
					{
						_insert_second_level(i, vo, pa, min(size, end-vo), flags, slab);
					}
				};

				/* check whether we wrap */
				if (end < vo) return;

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
		 * \param slab  second level page slab allocator
		 */
		void remove_translation(addr_t vo, size_t size, Page_slab * slab)
		{
			if (vo > (vo + size)) throw Invalid_range();

			for (unsigned i; (size > 0) && _index_by_vo(i, vo);) {

				addr_t end = (vo + Descriptor::VIRT_SIZE)
				             & Descriptor::VIRT_BASE_MASK;

				switch (Descriptor::type(_entries[i])) {

				case Descriptor::PAGE_TABLE:
					{
						typedef Page_table_descriptor Ptd;
						typedef Page_table            Pt;

						Pt * pt_phys = (Pt *) Ptd::Pa::masked(_entries[i]);
						Pt * pt      = (Pt *) slab->virt_addr(pt_phys);
						pt = pt ? pt : pt_phys; // TODO hack for core

						addr_t const pt_vo = vo - Section::Pa::masked(vo);
						pt->remove_translation(pt_vo, min(size, end-vo));

						if (pt->empty()) {
							Descriptor::invalidate(_entries[i]);
							destroy(slab, pt);
						}
						break;
					}

				default:
					{
						Descriptor::invalidate(_entries[i]);
					}
				}

				/* check whether we wrap */
				if (end < vo) return;

				size_t sz  = end - vo;
				size = (size > sz) ? size - sz : 0;
				vo += sz;
			}
		}
} __attribute__((aligned(1<<Section_table::ALIGNM_LOG2)));

namespace Genode { using Translation_table = Arm::Section_table; }

#endif /* _ARM__SHORT_TRANSLATION_TABLE_H_ */

