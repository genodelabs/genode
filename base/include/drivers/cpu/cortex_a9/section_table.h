/*
 * \brief   Driver for Cortex A9 section tables as software TLB
 * \author  Martin Stein
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__CPU__CORTEX_A9__SECTION_TABLE_H_
#define _INCLUDE__DRIVERS__CPU__CORTEX_A9__SECTION_TABLE_H_

/* Genode includes */
#include <util/register.h>
#include <base/printf.h>
#include <drivers/cpu/cortex_a9/core.h>

namespace Genode
{
	/**
	 * Check if 'p' is aligned to 1 << 'alignm_log2'
	 */
	bool inline aligned(addr_t const a, unsigned long const alignm_log2)
	{
		return a == ((a >> alignm_log2) << alignm_log2);
	}

	/**
	 * Common access permission [1:0] bitfield values
	 */
	struct Ap_1_0_bitfield
	{
		enum { KERNEL_AND_USER_NO_ACCESS = 0,
		       KERNEL_AND_USER_SAME_ACCESS = 3 };
	};

	/**
	 * Common access permission [2] bitfield values
	 */
	struct Ap_2_bitfield
	{
		enum { KERNEL_RW_OR_NO_ACCESS = 0,
		       KERNEL_RO_ACCESS       = 1 };
	};

	/**
	 * Cortex A9 second level translation table
	 *
	 * A table is dedicated to either secure or non-secure mode. All
	 * translations done by this table apply domain 0. They are not shareable
	 * and have zero-filled memory region attributes.
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
				enum Type { FAULT, SMALL_PAGE, LARGE_PAGE };

				struct Type_1 : Bitfield<1, 1> { };
				struct Type_2 : Bitfield<0, 1> { };

				/**
				 * Get descriptor type of 'v'
				 */
				static Type type(access_t const v)
				{
					access_t const t1 = Type_1::get(v);
					if (t1 == 0) {
						access_t const t2 = Type_2::get(v);
						if (t2 == 0) return FAULT;
						if (t2 == 1) return LARGE_PAGE;
					}
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
						Type_1::set(v, 0);
						Type_2::set(v, 0);
						break;
					case SMALL_PAGE:
						Type_1::set(v, 1);
						 break;
					case LARGE_PAGE:
						Type_1::set(v, 0);
						Type_2::set(v, 1);
						break;
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
			 * Large page descriptor structure
			 *
			 * Must always occur as group of 16 consecutive copies, this groups
			 * must be aligned on a 16 word boundary (Represents 64KB = 16 *
			 * Small page size)
			 */
			struct Large_page : Descriptor
			{
				enum { VIRT_SIZE_LOG2 = _64KB_LOG2,
					   VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					   VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1) };

				struct B        : Bitfield<2, 1> { };   /* part of the memory region attributes */
				struct C        : Bitfield<3, 1> { };   /* part of the memory region attributes */
				struct Ap_1_0   : Bitfield<4, 2>,       /* access permission bits [1:0] */
				                  Ap_1_0_bitfield { };
				struct Ap_2     : Bitfield<9, 1>,       /* access permission bits [2] */
				                  Ap_2_bitfield { };
				struct S        : Bitfield<10, 1> { };  /* shareable bit */
				struct Ng       : Bitfield<11, 1> { };  /* not global bit */
				struct Tex      : Bitfield<12, 3> { };  /* part of the memory region attributes */
				struct Xn       : Bitfield<15, 1> { };  /* execute never bit */
				struct Pa_31_16 : Bitfield<16, 16> { }; /* physical address bits [31:16] */
			};

			/**
			 * Small page descriptor structure
			 */
			struct Small_page : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2 = _4KB_LOG2,
					VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1)
				};

				struct Xn       : Bitfield<0, 1> { };   /* execute never bit */
				struct B        : Bitfield<2, 1> { };   /* part of the memory region attributes */
				struct C        : Bitfield<3, 1> { };   /* part of the memory region attributes */
				struct Ap_1_0   : Bitfield<4, 2>,       /* access permission bits [1:0] */
				                  Ap_1_0_bitfield { };
				struct Tex      : Bitfield<6, 3> { };   /* part of the memory region attributes */
				struct Ap_2     : Bitfield<9, 1>,       /* access permission bits [2] */
				                  Ap_2_bitfield { };
				struct S        : Bitfield<10, 1> { };  /* shareable bit */
				struct Ng       : Bitfield<11, 1> { };  /* not global bit */
				struct Pa_31_12 : Bitfield<12, 20> { }; /* physical address bits [31:12] */

				/**
				 * Permission configuration according to given access rights
				 *
				 * \param  r  readability
				 * \param  w  writeability
				 * \param  x  executability
				 * \return    descriptor value configured with appropriate
				 *            access permissions and the rest left zero
				 */
				static access_t access_permission_bits(bool const r,
				                                       bool const w,
				                                       bool const x)
				{
					access_t v = Xn::bits(!x);
					if (r) {
						v |= Ap_1_0::bits(Ap_1_0::KERNEL_AND_USER_SAME_ACCESS);
						if(w) v |= Ap_2::bits(Ap_2::KERNEL_RW_OR_NO_ACCESS);
						else v |= Ap_2::bits(Ap_2::KERNEL_RO_ACCESS);
					}
					else if (w) {
						PDBG("Write only translations not supported");
						while (1) ;
					}
					else {
						v |= Ap_1_0::bits(Ap_1_0::KERNEL_AND_USER_NO_ACCESS)
						     | Ap_2::bits(Ap_2::KERNEL_RO_ACCESS);
					}
					return v;
				}
			};

			/*
			 * Table payload
			 *
			 * Attention: Must be the only member of this class
			 */
			Descriptor::access_t _entries[SIZE/sizeof(Descriptor::access_t)];

			enum { MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1 };

			/**
			 * Get entry index by virtual offset
			 *
			 * \param  i    is overridden with the resulting index
			 * \param  vo   virtual offset relative to the virtual table base
			 * \retval  <0  if virtual offset couldn't be resolved,
			 *              in this case 'i' reside invalid
			 */
			int _index_by_vo (unsigned long & i, addr_t const vo) const
			{
				if (vo > max_virt_offset()) return -1;
				i = vo >> Small_page::VIRT_SIZE_LOG2;
				return 0;
			}

		public:

			/**
			 * Placement new operator
			 */
			void * operator new (size_t, void * p) { return p; }

			/**
			 * Constructor
			 */
			Page_table()
			{
				/* check table alignment */
				if (!aligned((addr_t)this, ALIGNM_LOG2)
				    || (addr_t)this != (addr_t)_entries) {

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
			 * \param  vo           offset of the virtual region represented
			 *                      by the translation within the virtual
			 *                      region represented by this table
			 * \param  pa           base of the physical backing store
			 * \param  size_log2    log2(Size of the translated region),
			 *                      must be supported by this table
			 * \param  r            shall one can read trough this translation
			 * \param  w            shall one can write trough this translation
			 * \param  x            shall one can execute trough this
			 *                      translation
			 *
			 * This method overrides an existing translation in case that it
			 * spans the the same virtual range and is not a link to another
			 * table level.
			 */
			void insert_translation (addr_t const vo, addr_t const pa,
			                         unsigned long const size_log2,
			                         bool const r, bool const w, bool const x,
			                         bool const global)
			{
				/* validate virtual address */
				unsigned long i;
				if (_index_by_vo (i, vo)) {
					PDBG("Invalid virtual offset");
					while (1) ;
				}

				/* select descriptor type by the translation size */
				if (size_log2 == Small_page::VIRT_SIZE_LOG2) {

					/*
					 * Can we write to the targeted entry?
					 */
					if (Descriptor::valid(_entries[i]) &&
					    Descriptor::type(_entries[i]) != Descriptor::SMALL_PAGE)
					{
						PDBG("Couldn't override entry");
						while (1) ;
					}

					/* compose descriptor */
					_entries[i] = Small_page::access_permission_bits(r, w, x)
					              | Small_page::Ng::bits(!global)
					              | Small_page::Pa_31_12::masked(pa);
					Descriptor::type(_entries[i], Descriptor::SMALL_PAGE);
					return;
				}
				PDBG("Translation size not supported");
				while (1) ;
			}

			/**
			 * Remove translations, wich overlap with a given virtual region
			 *
			 * \param   vo    offset of the virtual region within the region
			 *                represented by this table
			 * \param   size  region size
			 */
			void remove_region (addr_t const vo, size_t const size)
			{
				/* traverse all possibly affected entries */
				addr_t residual_vo = vo;
				unsigned long i;
				while (1)
				{
					/*
					 * Is anything left over to remove?
					 */
					if (residual_vo >= vo + size) return;

					/*
					 * Does the residual region overlap with the region
					 * represented by this table?
					 */
					if (_index_by_vo(i, residual_vo)) return;

					/*
					 * Update current entry and recalculate the residual
					 * region.
					 */
					switch (Descriptor::type(_entries[i]))
					{
					case Descriptor::FAULT:
					{
						residual_vo = (residual_vo & Fault::VIRT_BASE_MASK)
						              + Fault::VIRT_SIZE;
						break;
					}
					case Descriptor::SMALL_PAGE:
					{
						residual_vo = (residual_vo & Small_page::VIRT_BASE_MASK)
						              + Small_page::VIRT_SIZE;
						Descriptor::invalidate(_entries[i]);
						break;
					}
					case Descriptor::LARGE_PAGE:
					{
						PDBG("Removal of large pages not implemented");
						while (1) ;
						break;
					}
					}
				}
				return;
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

	} __attribute__((aligned(1<<Page_table::ALIGNM_LOG2)));

	/**
	 * Cortex A9 first level translation table
	 *
	 * A table is dedicated to either secure or non-secure mode. All
	 * translations done by this table apply domain 0. They are not shareable
	 * and have zero-filled memory region attributes. The size of this table is
	 * fixed to such a value that this table translates a space wich is
	 * addressable by 32 bit.
	 */
	class Section_table
	{
		enum {
			_16KB_LOG2 = 14,
			_1MB_LOG2 = 20,
			_16MB_LOG2 = 24,
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

				MAX_TRANSL_SIZE_LOG2 = 20,
				MIN_TRANSL_SIZE_LOG2 = 12,
			};

		protected:

			/**
			 * A first level translation descriptor
			 */
			struct Descriptor : Register<32>
			{
				/**
				 * Descriptor types
				 */
				enum Type { FAULT, PAGE_TABLE, SECTION, SUPERSECTION };

				struct Type_1 : Bitfield<0, 2> { };  /* entry type encoding 1 */
				struct Type_2 : Bitfield<18, 1> { }; /* entry type encoding 2 */

				/**
				 * Get descriptor type of 'v'
				 */
				static Type type(access_t const v)
				{
					access_t const t1 = Type_1::get(v);
					if (t1 == 0) return FAULT;
					if (t1 == 1) return PAGE_TABLE;
					if (t1 == 2) {
						access_t const t2 = Type_2::get(v);
						if (t2 == 0) return SECTION;
						if (t2 == 1) return SUPERSECTION;
					}
					return FAULT;
				}

				/**
				 * Set descriptor type of 'v'
				 */
				static void type(access_t & v, Type const t)
				{
					switch (t) {
					case FAULT: Type_1::set(v, 0); break;
					case PAGE_TABLE: Type_1::set(v, 1); break;
					case SECTION:
						Type_1::set(v, 2);
						Type_2::set(v, 0); break;
					case SUPERSECTION:
						Type_1::set(v, 2);
						Type_2::set(v, 1); break;
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
			 * References a second level translation table for the virtual
			 * region it represents
			 */
			struct Page_table_descriptor : Descriptor
			{
				struct Ns       : Bitfield<3, 1> { };   /* non-secure bit */
				struct Domain   : Bitfield<5, 4> { };   /* domain field */
				struct Pa_31_10 : Bitfield<10, 22> { }; /* physical address bits [31:10] */
			};

			/**
			 * Supersection-descriptor structure
			 *
			 * Must always occur as group of 16 consecutive copies, this groups
			 * must be aligned on a 16 word boundary.
			 */
			struct Supersection : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2 = _16MB_LOG2,
					VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1)
				};

				struct B        : Bitfield<2, 1> { };  /* part of the memory region attributes */
				struct C        : Bitfield<3, 1> { };  /* part of the memory region attributes */
				struct Xn       : Bitfield<4, 1> { };  /* execute never bit */
				struct Pa_39_36 : Bitfield<5, 4> { };  /* extendend physical address bits [39:36] */
				struct Ap_1_0   : Bitfield<10, 2>,     /* access permission bits [1:0] */
				                  Ap_1_0_bitfield { };
				struct Tex      : Bitfield<12, 3> { }; /* part of the memory region attributes */
				struct Ap_2     : Bitfield<15, 1>,     /* access permission bits [2] */
				                  Ap_2_bitfield { };
				struct S        : Bitfield<16, 1> { }; /* shareable bit */
				struct Ng       : Bitfield<17, 1> { }; /* not global bit */
				struct Ns       : Bitfield<19, 1> { }; /* non-secure bit */
				struct Pa_35_32 : Bitfield<20, 4> { }; /* extendend physical address bits [35:32] */
				struct Pa_31_24 : Bitfield<24, 8> { }; /* physical address bits [31:24] */
			};

			/**
			 * Section-descriptor structure
			 */
			struct Section : Descriptor
			{
				enum {
					VIRT_SIZE_LOG2 = _1MB_LOG2,
					VIRT_SIZE = 1 << VIRT_SIZE_LOG2,
					VIRT_BASE_MASK = ~((1 << VIRT_SIZE_LOG2) - 1)
				};

				struct B        : Bitfield<2, 1> { }; /* part of the memory region attributes */
				struct C        : Bitfield<3, 1> { }; /* part of the memory region attributes */
				struct Xn       : Bitfield<4, 1> { }; /* execute never bit */
				struct Domain   : Bitfield<5, 4> { }; /* domain field */
				struct Ap_1_0   : Bitfield<10, 2>,    /* access permission bits [1:0] */
				                  Ap_1_0_bitfield { };
				struct Tex      : Bitfield<12, 3> { }; /* part of the memory region attributes */
				struct Ap_2     : Bitfield<15, 1>,     /* access permission bits [2] */
				                  Ap_2_bitfield { };
				struct S        : Bitfield<16, 1> { };  /* shareable bit */
				struct Ng       : Bitfield<17, 1> { };  /* not global bit */
				struct Ns       : Bitfield<19, 1> { };  /* non-secure bit */
				struct Pa_31_20 : Bitfield<20, 12> { }; /* physical address bits [31:20] */

				/**
				 * Permission configuration according to given access rights
				 *
				 * \param  r  readability
				 * \param  w  writeability
				 * \param  x  executability
				 * \return    descriptor value configured with appropriate
				 *            access permissions and the rest left zero
				 */
				static access_t access_permission_bits(bool const r,
				                                       bool const w,
				                                       bool const x)
				{
					access_t v = Xn::bits(!x);
					if (r) {
						v |= Ap_1_0::bits(Ap_1_0::KERNEL_AND_USER_SAME_ACCESS);
						if(w) v |= Ap_2::bits(Ap_2::KERNEL_RW_OR_NO_ACCESS);
						else v |= Ap_2::bits(Ap_2::KERNEL_RO_ACCESS);
					}
					else if (w) {
						PDBG("Write only sections not supported");
						while (1) ;
					}
					else {
						v |= Ap_1_0::bits(Ap_1_0::KERNEL_AND_USER_NO_ACCESS)
						     | Ap_2::bits(Ap_2::KERNEL_RW_OR_NO_ACCESS);
					}
					return v;
				}
			};

			/*
			 * Table payload
			 *
			 * Attention: Must be the first member of this class
			 */
			Descriptor::access_t _entries[SIZE/sizeof(Descriptor::access_t)];

			enum { MAX_INDEX = sizeof(_entries) / sizeof(_entries[0]) - 1 };

			/* is this table dedicated to secure mode or to non-secure mode */
			bool _secure;

			/**
			 * Get entry index by virtual offset
			 *
			 * \param  i    is overridden with the resulting index
			 * \param  vo   offset within the virtual region represented
			 *              by this table
			 * \retval <0   if virtual offset couldn't be resolved,
			 *              in this case 'i' reside invalid
			 */
			int _index_by_vo(unsigned long & i, addr_t const vo) const
			{
				if (vo > max_virt_offset()) return -1;
				i = vo >> Section::VIRT_SIZE_LOG2;
				return 0;
			}

		public:

			/**
			 * Constructor for a table that adopts current secure mode status
			 */
			Section_table() : _secure(Cortex_a9::secure_mode_active())
			{
				/* check table alignment */
				if (!aligned((addr_t)this, ALIGNM_LOG2)
				    || (addr_t)this != (addr_t)_entries) {

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
			 * \param  vo           offset of the virtual region represented
			 *                      by the translation within the virtual
			 *                      region represented by this table
			 * \param  pa           base of the physical backing store
			 * \param  size_log2    size log2 of the translated region
			 * \param  r            shall one can read trough this translation
			 * \param  w            shall one can write trough this translation
			 * \param  x            shall one can execute trough this translation
			 * \param  global       shall the translation apply to all
			 *                      address spaces
			 * \param  extra_space  If > 0, it must point to a portion of
			 *                      size-aligned memory space wich may be used
			 *                      furthermore by the table for the incurring
			 *                      administrative costs of the  translation.
			 *                      To determine the amount of additionally
			 *                      needed memory one can instrument this
			 *                      method with 'extra_space' set to 0.
			 *                      The so donated memory may be regained by
			 *                      using the method 'regain_memory'.
			 * \retval  0           translation successfully inserted
			 * \retval  >0          translation not inserted, the return value
			 *                      is the size log2 of additional size-aligned
			 *                      space that is needed to do the translation.
			 *                      This occurs solely when 'extra_space' is 0.
			 *
			 * This method overrides an existing translation in case that it
			 * spans the the same virtual range and is not a link to another
			 * table level.
			 */
			unsigned long insert_translation (addr_t const vo, addr_t const pa,
			                                  unsigned long const size_log2,
			                                  bool const r, bool const w,
			                                  bool const x, bool const global,
			                                  void * const extra_space = 0)
			{
				/* validate virtual address */
				unsigned long i;
				if (_index_by_vo (i, vo)) {
					PDBG("Invalid virtual offset");
					while (1) ;
				}

				/* select descriptor type by translation size */
				if (size_log2 < Section::VIRT_SIZE_LOG2) {

					Page_table * pt;

					/*
					 * Does an appropriate page table already exist?
					 */
					if (Descriptor::type(_entries[i]) == Descriptor::PAGE_TABLE) {
						pt = (Page_table *)(addr_t)
							Page_table_descriptor::Pa_31_10::masked(_entries[i]);
					}

					/*
					 * Is there some extra space to create a page table?
					 */
					else if (extra_space) {

						/*
						 * Can we write to the targeted entry?
						 */
						if (Descriptor::valid(_entries[i])) {
							PDBG ("Couldn't override entry");
							while (1) ;
						}
						/*
						 * Create and link page table. The page table checks
						 * alignment by itself.
						 */
						pt = new (extra_space) Page_table();
						_entries[i] = Page_table_descriptor::Ns::bits(!_secure)
						              | Page_table_descriptor::Pa_31_10::masked((addr_t)pt);
						Descriptor::type(_entries[i], Descriptor::PAGE_TABLE);
					}
					/* Request additional memory to create a page table */
					else return Page_table::SIZE_LOG2;

					/* insert translation */
					pt->insert_translation(vo - Section::Pa_31_20::masked(vo),
					                       pa, size_log2, r, w, x, global);
					return 0;
				}
				if (size_log2 == Section::VIRT_SIZE_LOG2) {

					/*
					 * Can we write to the targeted entry?
					 */
					if (Descriptor::valid(_entries[i]) &&
					    Descriptor::type(_entries[i]) != Descriptor::SECTION)
					{
						PDBG("Couldn't override entry");
						while (1) ;
					}

					/* compose section descriptor */
					_entries[i] = Section::access_permission_bits(r, w, x)
					              | Section::Ns::bits(!_secure)
					              | Section::Ng::bits(!global)
					              | Section::Pa_31_20::masked(pa);
					Descriptor::type(_entries[i], Descriptor::SECTION);
					return 0;
				}
				PDBG("Translation size not supported");
				while (1) ;
			}

			/**
			 * Remove translations, wich overlap with a given virtual region
			 *
			 * \param  vo    offset of the virtual region within the region
			 *               represented by this table
			 * \param  size  region size
			 */
			void remove_region (addr_t const vo, size_t const size)
			{
				/* traverse all possibly affected entries */
				addr_t residual_vo = vo;
				unsigned long i;
				while (1)
				{
					/*
					 * Is anything left over to remove?
					 */
					if (residual_vo >= vo + size) return;

					/*
					 * Does the residual region overlap with the region
					 * represented by this table?
					 */
					if (_index_by_vo(i, residual_vo)) return;

					/*
					 * Update current entry and recalculate the residual
					 * region.
					 */
					switch (Descriptor::type(_entries[i]))
					{
					case Descriptor::FAULT:
					{
						residual_vo = (residual_vo & Fault::VIRT_BASE_MASK)
						              + Fault::VIRT_SIZE;
						break;
					}
					case Descriptor::PAGE_TABLE:
					{
						/* instruct page table to remove residual region */
						Page_table * const pt = (Page_table *)
						(addr_t)Page_table_descriptor::Pa_31_10::masked(_entries[i]);
						size_t const residual_size = vo + size - residual_vo;
						addr_t const pt_vo = residual_vo
						                     - Section::Pa_31_20::masked(residual_vo);
						pt->remove_region(pt_vo, residual_size);

						/* Recalculate residual region */
						residual_vo = (residual_vo & Page_table::VIRT_BASE_MASK)
						              + Page_table::VIRT_SIZE;
						break;
					}
					case Descriptor::SECTION:
					{
						Descriptor::invalidate(_entries[i]);
						residual_vo = (residual_vo & Section::VIRT_BASE_MASK)
						              + Section::VIRT_SIZE;
						break;
					}
					case Descriptor::SUPERSECTION:
					{
						PDBG("Removal of supersections not implemented");
						while (1);
						break;
					}
					}
				}
			}

			/**
			 * Get a portion of memory that is no longer used by this table
			 *
			 * \param   base  base of regained memory portion if method returns 1
			 * \param   s     size of regained memory portion if method returns 1
			 */
			bool regain_memory (void * & base, size_t & s)
			{
				/* walk through all entries */
				for (unsigned i = 0; i <= MAX_INDEX; i++)
				{
					if (Descriptor::type(_entries[i]) == Descriptor::PAGE_TABLE) {

						Page_table * const pt = (Page_table *)
						(addr_t)Page_table_descriptor::Pa_31_10::masked(_entries[i]);

						if (pt->empty()) {

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
	} __attribute__((aligned(1<<Section_table::ALIGNM_LOG2)));
}

#endif /* _INCLUDE__DRIVERS__CPU__CORTEX_A9__SECTION_TABLE_H_ */

