/*
 * \brief   TLB driver for core
 * \author  Martin Stein
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TLB__ARM_H_
#define _TLB__ARM_H_

/* Genode includes */
#include <util/register.h>
#include <base/printf.h>

/* base-hw includes */
#include <placement_new.h>
#include <tlb/page_flags.h>

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

	/**
	 * Second level translation table
	 *
	 * A table is dedicated to either secure or non-secure mode. All
	 * translations done by this table apply to domain 0. They are not
	 * shareable and have zero-filled memory region attributes.
	 */
	class Page_table
	{
		enum {
			_1KB_LOG2 = 10,
			_4KB_LOG2 = 12,
			_64KB_LOG2 = 16,
			_1MB_LOG2 = 20,
		};

		public:

			enum {
				SIZE_LOG2 = _1KB_LOG2,
				SIZE = 1 << SIZE_LOG2,
				ALIGNM_LOG2 = SIZE_LOG2,

				VIRT_SIZE_LOG2 = _1MB_LOG2,
				VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
				VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1),
			};

		protected:

			/**
			 * Common descriptor structure
			 */
			struct Descriptor : Register<32>
			{
				/**
				 * Descriptor types
				 */
				enum Type { FAULT, SMALL_PAGE };

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
				static bool valid(access_t & v) { return type(v) != FAULT; }
			};

			/**
			 * Represents an untranslated virtual region
			 */
			struct Fault : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2 = _4KB_LOG2,
					VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1)
				};
			};

			/**
			 * Small page descriptor structure
			 */
			struct Small_page : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2   = _4KB_LOG2,
					VIRT_SIZE        = 1 << VIRT_SIZE_LOG2,
					VIRT_OFFSET_MASK = (1 << VIRT_SIZE_LOG2) - 1,
					VIRT_BASE_MASK   = ~(VIRT_OFFSET_MASK),
				};

				struct Xn       : Bitfield<0, 1> { };   /* execute never */
				struct B        : Bitfield<2, 1> { };   /* mem region attr. */
				struct C        : Bitfield<3, 1> { };   /* mem region attr. */
				struct Ap_0     : Bitfield<4, 2> { };   /* access permission */
				struct Tex      : Bitfield<6, 3> { };   /* mem region attr. */
				struct Ap_1     : Bitfield<9, 1> { };   /* access permission */
				struct S        : Bitfield<10, 1> { };  /* shareable bit */
				struct Ng       : Bitfield<11, 1> { };  /* not global bit */
				struct Pa_31_12 : Bitfield<12, 20> { }; /* physical base */

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
					v |= Pa_31_12::masked(pa);
					Descriptor::type(v, Descriptor::SMALL_PAGE);
					return v;
				}
			};

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
			int _index_by_vo (unsigned & i, addr_t const vo) const
			{
				if (vo > max_virt_offset()) return -1;
				i = vo >> Small_page::VIRT_SIZE_LOG2;
				return 0;
			}

		public:

			/**
			 * Constructor
			 */
			Page_table()
			{
				/* check table alignment */
				if (!aligned((addr_t)this, ALIGNM_LOG2) ||
				    (addr_t)this != (addr_t)_entries)
				{
					PDBG("Insufficient table alignment");
					while (1) ;
				}
				/* start with an empty table */
				for (unsigned i = 0; i <= MAX_INDEX; i++)
					Descriptor::invalidate(_entries[i]);
			}

			/**
			 * Maximum virtual offset that can be translated by this table
			 */
			static addr_t max_virt_offset()
			{
				return (MAX_INDEX << Small_page::VIRT_SIZE_LOG2)
				       + (Small_page::VIRT_SIZE - 1);
			}

			/**
			 * Insert one atomic translation into this table
			 *
			 * \param vo         offset of the virtual region represented
			 *                   by the translation within the virtual
			 *                   region represented by this table
			 * \param pa         base of the physical backing store
			 * \param size_log2  log2(Size of the translated region),
			 *                   must be supported by this table
			 * \param flags      mapping flags
			 *
			 * This method overrides an existing translation in case
			 * that it spans the the same virtual range and is not
			 * a link to another table level.
			 */
			void insert_translation(addr_t const vo, addr_t const pa,
			                        size_t const size_log2,
			                        Page_flags const & flags)
			{
				/* validate virtual address */
				unsigned i;
				if (_index_by_vo (i, vo)) {
					PDBG("Invalid virtual offset");
					while (1) ;
				}
				/* select descriptor type by the translation size */
				if (size_log2 == Small_page::VIRT_SIZE_LOG2)
				{
					/* compose new descriptor value */
					Descriptor::access_t const entry =
						Small_page::create(flags, pa);

					/* check if we can we write to the targeted entry */
					if (Descriptor::valid(_entries[i]))
					{
						/*
						 * It's possible that multiple threads fault at the
						 * same time on the same translation, thus we need
						 * this check.
						 */
						if (_entries[i] == entry) return;

						/* never modify existing translations */
						PDBG("Couldn't override entry");
						while (1) ;
					}
					/* override table entry with new descriptor value */
					_entries[i] = entry;
					return;
				}
				PDBG("Translation size not supported");
				while (1) ;
			}

			/**
			 * Remove translations that overlap with a given virtual region
			 *
			 * \param vo    region offset within the tables virtual region
			 * \param size  region size
			 */
			void remove_region(addr_t vo, size_t const size)
			{
				addr_t const ve = vo + size;
				unsigned i;
				while (1)
				{
					if (vo >= ve) { return; }
					if (_index_by_vo(i, vo)) { return; }
					addr_t next_vo;
					switch (Descriptor::type(_entries[i])) {

					case Descriptor::FAULT: {

						vo &= Fault::VIRT_BASE_MASK;
						next_vo = vo + Fault::VIRT_SIZE;
						break; }

					case Descriptor::SMALL_PAGE: {

						vo &= Small_page::VIRT_BASE_MASK;
						Descriptor::invalidate(_entries[i]);
						next_vo = vo + Small_page::VIRT_SIZE;
						break; }
					}
					if (next_vo < vo) { return; }
					vo = next_vo;
				}
			}

			/**
			 * Does this table solely contain invalid entries
			 */
			bool empty()
			{
				for (unsigned i = 0; i <= MAX_INDEX; i++) {
					if (Descriptor::valid(_entries[i])) return false;
				}
				return true;
			}

			/**
			 * Get next translation size log2 by area constraints
			 *
			 * \param vo  virtual offset within this table
			 * \param s   area size
			 */
			static unsigned
			translation_size_l2(addr_t const vo, size_t const s)
			{
				off_t const o = vo & Small_page::VIRT_OFFSET_MASK;
				if (!o && s >= Small_page::VIRT_SIZE)
					return Small_page::VIRT_SIZE_LOG2;
				PDBG("Insufficient alignment or size");
				while (1) ;
			}

	} __attribute__((aligned(1<<Page_table::ALIGNM_LOG2)));

	/**
	 * First level translation table
	 *
	 * A table is dedicated to either secure or non-secure mode. All
	 * translations done by this table apply to domain 0. They are not
	 * shareable and have zero-filled memory region attributes. The size
	 * of this table is fixed to such a value that this table translates
	 * a space wich is addressable by 32 bit.
	 */
	class Section_table
	{
		enum {
			_16KB_LOG2 = 14,
			_1MB_LOG2 = 20,
			_16MB_LOG2 = 24,

			DOMAIN = 0,
		};

		public:

			enum {
				SIZE_LOG2 = _16KB_LOG2,
				SIZE = 1 << SIZE_LOG2,
				ALIGNM_LOG2 = SIZE_LOG2,

				VIRT_SIZE_LOG2 = _1MB_LOG2,
				VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
				VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1),

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

				struct Type_0   : Bitfield<0, 2> { };
				struct Type_1_0 : Bitfield<1, 1> { };
				struct Type_1_1 : Bitfield<18, 1> { };
				struct Type_1   : Bitset_2<Type_1_0, Type_1_1> { };

				/**
				 * Get descriptor type of 'v'
				 */
				static Type type(access_t const v)
				{
					access_t const t0 = Type_0::get(v);
					if (t0 == 0) { return FAULT; }
					if (t0 == 1) { return PAGE_TABLE; }
					access_t const t1 = Type_1::get(v);
					if (t1 == 1) { return SECTION; }
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
			};

			/**
			 * Represents an untranslated virtual region
			 */
			struct Fault : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2 = _1MB_LOG2,
					VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1)
				};
			};

			/**
			 * Link to a second level translation table
			 */
			struct Page_table_descriptor : Descriptor
			{
				struct Domain   : Bitfield<5, 4> { };   /* domain */
				struct Pa_31_10 : Bitfield<10, 22> { }; /* physical base */

				/**
				 * Compose descriptor value
				 */
				static access_t create(Page_table * const pt)
				{
						access_t v = Domain::bits(DOMAIN) |
						             Pa_31_10::masked((addr_t)pt);
						Descriptor::type(v, Descriptor::PAGE_TABLE);
						return v;
				}
			};

			/**
			 * Section translation descriptor
			 */
			struct Section : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2   = _1MB_LOG2,
					VIRT_SIZE        = 1 << VIRT_SIZE_LOG2,
					VIRT_OFFSET_MASK = (1 << VIRT_SIZE_LOG2) - 1,
					VIRT_BASE_MASK   = ~(VIRT_OFFSET_MASK),
				};

				struct B        : Bitfield<2, 1> { };   /* mem. region attr. */
				struct C        : Bitfield<3, 1> { };   /* mem. region attr. */
				struct Xn       : Bitfield<4, 1> { };   /* execute never bit */
				struct Domain   : Bitfield<5, 4> { };   /* domain */
				struct Ap_0     : Bitfield<10, 2> { };  /* access permission */
				struct Tex      : Bitfield<12, 3> { };  /* mem. region attr. */
				struct Ap_1     : Bitfield<15, 1> { };  /* access permission */
				struct S        : Bitfield<16, 1> { };  /* shared */
				struct Ng       : Bitfield<17, 1> { };  /* not global */
				struct Pa_31_20 : Bitfield<20, 12> { }; /* physical base */

				struct Ap : Bitset_2<Ap_0, Ap_1> { }; /* access permission */

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
					v |= Pa_31_20::masked(pa);
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
			int _index_by_vo(unsigned & i, addr_t const vo) const
			{
				if (vo > max_virt_offset()) return -1;
				i = vo >> Section::VIRT_SIZE_LOG2;
				return 0;
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
				/* check for appropriate positioning of the table */
				if (!aligned((addr_t)this, ALIGNM_LOG2)
				    || (addr_t)this != (addr_t)_entries)
				{
					PDBG("Insufficient table alignment");
					while (1) ;
				}
				/* start with an empty table */
				for (unsigned i = 0; i <= MAX_INDEX; i++)
					Descriptor::invalidate(_entries[i]);
			}

			/**
			 * Maximum virtual offset that can be translated by this table
			 */
			static addr_t max_virt_offset()
			{
				return (MAX_INDEX << Section::VIRT_SIZE_LOG2)
				       + (Section::VIRT_SIZE - 1);
			}

			/**
			 * Insert one atomic translation into this table
			 *
			 * \param ST           platform specific section-table type
			 * \param st           platform specific section table
			 * \param vo           offset of the virtual region represented
			 *                     by the translation within the virtual
			 *                     region represented by this table
			 * \param pa           base of the physical backing store
			 * \param size_log2    size log2 of the translated region
			 * \param flags        mapping flags
			 * \param extra_space  If > 0, it must point to a portion of
			 *                     size-aligned memory space wich may be used
			 *                     furthermore by the table for the incurring
			 *                     administrative costs of the  translation.
			 *                     To determine the amount of additionally
			 *                     needed memory one can instrument this
			 *                     method with 'extra_space' set to 0.
			 *                     The so donated memory may be regained by
			 *                     using the method 'regain_memory'.
			 *
			 * \retval  0  translation successfully inserted
			 * \retval >0  Translation not inserted, the return value
			 *             is the size log2 of additional size-aligned
			 *             space that is needed to do the translation.
			 *             This occurs solely when 'extra_space' is 0.
			 *
			 * This method overrides an existing translation in case that it
			 * spans the the same virtual range and is not a link to another
			 * table level.
			 */
			template <typename ST>
			size_t insert_translation(addr_t const vo, addr_t const pa,
			                          size_t const size_log2,
			                          Page_flags const & flags,
			                          ST * const st,
			                          void * const extra_space = 0)
			{
				typedef typename ST::Section Section;
				typedef typename ST::Page_table_descriptor Page_table_descriptor;

				/* validate virtual address */
				unsigned i;
				if (_index_by_vo (i, vo)) {
					PDBG("Invalid virtual offset");
					while (1) ;
				}
				/* select descriptor type by translation size */
				if (size_log2 < Section::VIRT_SIZE_LOG2)
				{
					/* check if an appropriate page table already exists */
					Page_table * pt;
					if (Descriptor::type(_entries[i]) == Descriptor::PAGE_TABLE) {
						pt = (Page_table *)(addr_t)
							Page_table_descriptor::Pa_31_10::masked(_entries[i]);
					}
					/* check if we have enough memory for the page table */
					else if (extra_space)
					{
						/* check if we can write to the targeted entry */
						if (Descriptor::valid(_entries[i])) {
							PDBG ("Couldn't override entry");
							while (1) ;
						}
						/* create and link page table */
						pt = new (extra_space) Page_table();
						_entries[i] = Page_table_descriptor::create(pt, st);
					}
					/* request additional memory to create a page table */
					else return Page_table::SIZE_LOG2;

					/* insert translation */
					pt->insert_translation(vo - Section::Pa_31_20::masked(vo),
					                       pa, size_log2, flags);
					return 0;
				}
				if (size_log2 == Section::VIRT_SIZE_LOG2)
				{
					/* compose section descriptor */
					Descriptor::access_t const entry =
						Section::create(flags, pa, st);

					/* check if we can we write to the targeted entry */
					if (Descriptor::valid(_entries[i]))
					{
						/*
						 * It's possible that multiple threads fault at the
						 * same time on the same translation, thus we need
						 * this check.
						 */
						if (_entries[i] == entry) return 0;

						/* never modify existing translations */
						PDBG("Couldn't override entry");
						while (1) ;
					}
					/* override the table entry */
					_entries[i] = entry;
					return 0;
				}
				PDBG("Translation size not supported");
				while (1) ;
			}

			/**
			 * Remove translations that overlap with a given virtual region
			 *
			 * \param vo    region offset within the tables virtual region
			 * \param size  region size
			 */
			void remove_region(addr_t vo, size_t const size)
			{
				addr_t const ve = vo + size;
				unsigned i;
				while (1)
				{
					if (vo >= ve) { return; }
					if (_index_by_vo(i, vo)) { return; }
					addr_t next_vo;
					switch (Descriptor::type(_entries[i])) {

					case Descriptor::FAULT: {

						vo &= Fault::VIRT_BASE_MASK;
						next_vo = vo + Fault::VIRT_SIZE;
						break; }

					case Descriptor::PAGE_TABLE: {

						typedef Page_table_descriptor Ptd;
						typedef Page_table            Pt;

						vo &= Pt::VIRT_BASE_MASK;
						Pt * const pt = (Pt *)Ptd::Pa_31_10::masked(_entries[i]);
						addr_t const pt_vo = vo - Section::Pa_31_20::masked(vo);
						pt->remove_region(pt_vo, ve - vo);
						next_vo = vo + Pt::VIRT_SIZE;
						break; }

					case Descriptor::SECTION: {

						vo &= Section::VIRT_BASE_MASK;
						Descriptor::invalidate(_entries[i]);
						next_vo = vo + Section::VIRT_SIZE;
						break; }
					}
					if (next_vo < vo) { return; }
					vo = next_vo;
				}
			}

			/**
			 * Get base address for hardware table walk
			 */
			addr_t base() const { return (addr_t)_entries; }

			/**
			 * Get a portion of memory that is no longer used by this table
			 *
			 * \param base  base of regained mem portion if method returns 1
			 * \param s     size of regained mem portion if method returns 1
			 *
			 * \retval 1  successfully regained memory
			 * \retval 0  no more memory to regain
			 */
			bool regain_memory (void * & base, size_t & s)
			{
				/* walk through all entries */
				for (unsigned i = 0; i <= MAX_INDEX; i++)
				{
					if (Descriptor::type(_entries[i]) == Descriptor::PAGE_TABLE)
					{
						Page_table * const pt = (Page_table *)
						(addr_t)Page_table_descriptor::Pa_31_10::masked(_entries[i]);
						if (pt->empty())
						{
							/* we've found an useless page table */
							Descriptor::invalidate(_entries[i]);
							base = (void *)pt;
							s = sizeof(Page_table);
							return true;
						}
					}
				}
				return false;
			}

			/**
			 * Get next translation size log2 by area constraints
			 *
			 * \param vo  virtual offset within this table
			 * \param s   area size
			 */
			static unsigned
			translation_size_l2(addr_t const vo, size_t const s)
			{
				off_t const o = vo & Section::VIRT_OFFSET_MASK;
				if (!o && s >= Section::VIRT_SIZE)
					return Section::VIRT_SIZE_LOG2;
				return Page_table::translation_size_l2(o, s);
			}

			/**
			 * Insert translations for given area, do not permit displacement
			 *
			 * \param vo     virtual offset within this table
			 * \param s      area size
			 * \param flags  mapping flags
			 */
			template <typename ST>
			void map_core_area(addr_t vo, size_t s, bool io_mem, ST * st)
			{
				/* initialize parameters */
				Page_flags const flags = Page_flags::map_core_area(io_mem);
				unsigned tsl2 = translation_size_l2(vo, s);
				size_t ts = 1 << tsl2;

				/* walk through the area and map all offsets */
				while (1)
				{
					/* map current offset without displacement */
					if(st->insert_translation(vo, vo, tsl2, flags)) {
						PDBG("Displacement not permitted");
						return;
					}
					/* update parameters for next round or exit */
					vo += ts;
					s = ts < s ? s - ts : 0;
					if (!s) return;
					tsl2 = translation_size_l2(vo, s);
					ts   = 1 << tsl2;
				}
			}

	} __attribute__((aligned(1<<Section_table::ALIGNM_LOG2)));
}

#endif /* _TLB__ARM_H_ */

