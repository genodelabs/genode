/*
 * \brief  Syscall bindings for the NOVA microhypervisor
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-12-27
 */

/*
 * Copyright (c) 2009 Genode Labs
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _PLATFORM__NOVA_SYSCALLS_GENERIC_H_
#define _PLATFORM__NOVA_SYSCALLS_GENERIC_H_

#include <nova/stdint.h>

namespace Nova {

	enum {
		PAGE_SIZE_LOG2 = 12,
		PAGE_SIZE_BYTE = 1 << PAGE_SIZE_LOG2,
		PAGE_MASK_ = ~(PAGE_SIZE_BYTE - 1)
	};

	/**
	 * NOVA system-call IDs
	 */
	enum Syscall {
		NOVA_CALL       = 0x0,
		NOVA_REPLY      = 0x1,
		NOVA_CREATE_PD  = 0x2,
		NOVA_CREATE_EC  = 0x3,
		NOVA_CREATE_SC  = 0x4,
		NOVA_CREATE_PT  = 0x5,
		NOVA_CREATE_SM  = 0x6,
		NOVA_REVOKE     = 0x7,
		NOVA_LOOKUP     = 0x8,
		NOVA_EC_CTRL    = 0x9,
		NOVA_SC_CTRL    = 0xa,
		NOVA_SM_CTRL    = 0xb,
		NOVA_ASSIGN_PCI = 0xc,
		NOVA_ASSIGN_GSI = 0xd,
	};

	/**
	 * NOVA status codes returned by system-calls
	 */
        enum Status
        {
            NOVA_OK             = 0,
            NOVA_IPC_TIMEOUT    = 1,
            NOVA_IPC_ABORT      = 2,
            NOVA_INV_HYPERCALL  = 3,
            NOVA_INV_SELECTOR   = 4,
            NOVA_INV_PARAMETER  = 5,
            NOVA_INV_FEATURE    = 6,
            NOVA_INV_CPU_NUMBER = 7,
            NOVA_INVD_DEVICE_ID = 8,
        };

	/**
	 * Hypervisor information page
	 */
	struct Hip
	{
		struct Mem_desc
		{
			enum Type {
				MULTIBOOT_MODULE    = -2,
				MICROHYPERVISOR     = -1,
				AVAILABLE_MEMORY    =  1,
				RESERVED_MEMORY     =  2,
				ACPI_RECLAIM_MEMORY =  3,
				ACPI_NVS_MEMORY     =  4
			};

			uint64_t const addr;
			uint64_t const size;
			Type     const type;
			uint32_t const aux;
		};

		uint32_t const signature;   /* magic value 0x41564f4e */
		uint16_t const hip_checksum;
		uint16_t const hip_length;
		uint16_t const cpu_desc_offset;
		uint16_t const cpu_desc_size;
		uint16_t const mem_desc_offset;
		uint16_t const mem_desc_size;
		uint32_t const feature_flags;
		uint32_t const api_version;
		uint32_t const sel;         /* number of cap selectors                 */
		uint32_t const sel_exc;     /* number of cap selectors for exceptions  */
		uint32_t const sel_vm;      /* number of cap selectors for VM handling */
		uint32_t const sel_gsi;     /* number of global system interrupts      */
		uint32_t const page_sizes;  /* supported page sizes                    */
		uint32_t const utcb_sizes;  /* supported utcb sizes                    */
		uint32_t const tsc_freq;    /* time-stamp counter frequency in kHz     */
		uint32_t const bus_freq;    /* bus frequency in kHz                    */

		bool has_feature_vmx() const { return feature_flags & (1 << 1); }
		bool has_feature_svm() const { return feature_flags & (1 << 2); }
	} __attribute__((packed));


	/**
	 * Semaphore operations
	 */
	enum Sem_op { SEMAPHORE_UP = 0U, SEMAPHORE_DOWN = 1U, SEMAPHORE_DOWNZERO = 0x3U };


	class Descriptor
	{
		protected:

			mword_t _value;

			/**
			 * Assign bitfield to descriptor
			 */
			template<mword_t MASK, mword_t SHIFT>
			void _assign(mword_t new_bits)
			{
				_value &= ~(MASK << SHIFT);
				_value |= (new_bits & MASK) << SHIFT;
			}

			/**
			 * Query bitfield from descriptor
			 */
			template<mword_t MASK, mword_t SHIFT>
			mword_t _query() const { return (_value >> SHIFT) & MASK; }

		public:

			mword_t value() const { return _value; }
	} __attribute__((packed));


	/**
	 * Message-transfer descriptor
	 */
	class Mtd
	{
		private:

			mword_t const _value;

		public:

			enum {
				ACDB = 1 << 0,   /* eax, ecx, edx, ebx */
				ESP  = 1 << 2,
				EIP  = 1 << 3,
				EFL  = 1 << 4,   /* eflags */
				QUAL = 1 << 15,  /* exit qualification */
				CTRL = 1 << 16,  /* execution controls */
				INJ  = 1 << 17,  /* injection info */
				STA  = 1 << 18,  /* interruptibility state */
				TSC  = 1 << 19,  /* time-stamp counter */

				IRQ  = EFL | STA | INJ | TSC,
				ALL  = 0x000fffff & ~CTRL,
			};

			Mtd(mword_t value) : _value(value) { }

			mword_t value() const { return _value; }
	};


	class Crd : public Descriptor
	{
		protected:

			/**
			 * Bitfield holding the descriptor type
			 */
			enum {
				TYPE_MASK   = 0x3,  TYPE_SHIFT  =  0,
				BASE_SHIFT  = 12,   RIGHTS_MASK = 0x1f,
				ORDER_MASK  = 0x1f, ORDER_SHIFT =  7,
				BASE_MASK   = (~0UL) >> BASE_SHIFT,
				RIGHTS_SHIFT= 2
			};

			/**
			 * Capability-range-descriptor types
			 */
			enum {
				NULL_CRD_TYPE   = 0,
				MEM_CRD_TYPE    = 1,
				IO_CRD_TYPE     = 2,
				OBJ_CRD_TYPE    = 3,
				RIGHTS_ALL      = 0x1f,
			};

			void _base(mword_t base)
			{ _assign<BASE_MASK, BASE_SHIFT>(base); }

			void _order(mword_t order)
			{ _assign<ORDER_MASK, ORDER_SHIFT>(order); }

		public:

			Crd(mword_t base, mword_t order) {
				_value = 0; _base(base), _order(order); }

			Crd(mword_t value) { _value = value; }

			mword_t hotspot(mword_t sel_hotspot) const
			{
				if ((value() & TYPE_MASK) == MEM_CRD_TYPE)
					return sel_hotspot & PAGE_MASK_;

				return sel_hotspot << 12;
			}

			mword_t addr()  const { return base() << BASE_SHIFT; }
			mword_t base()  const { return _query<BASE_MASK, BASE_SHIFT>(); }
			mword_t order() const { return _query<ORDER_MASK, ORDER_SHIFT>(); }
			bool is_null()  const { return (_value & TYPE_MASK) == NULL_CRD_TYPE; }
			uint8_t type()  const { return _query<TYPE_MASK, TYPE_SHIFT>(); }
			uint8_t rights() const { return _query<RIGHTS_MASK, RIGHTS_SHIFT>(); }
	} __attribute__((packed));


	class Rights
	{
		private:

			bool const _readable, _writeable, _executable;

		public:

			Rights(bool readable, bool writeable, bool executable)
			: _readable(readable), _writeable(writeable),
			  _executable(executable) { }

			Rights() : _readable(false), _writeable(false), _executable(false) {}

			bool readable()   const { return _readable; }
			bool writeable()  const { return _writeable; }
			bool executable() const { return _executable; }
	};


	/**
	 * Memory-capability-range descriptor
	 */
	class Mem_crd : public Crd
	{
		private:

			enum {
				EXEC_MASK  = 0x1, EXEC_SHIFT  =  4,
				WRITE_MASK = 0x1, WRITE_SHIFT =  3,
				READ_MASK  = 0x1, READ_SHIFT  =  2
			};

			void _rights(Rights r)
			{
				_assign<EXEC_MASK,  EXEC_SHIFT>(r.executable());
				_assign<WRITE_MASK, WRITE_SHIFT>(r.writeable());
				_assign<READ_MASK,  READ_SHIFT>(r.readable());
			}

		public:

			Mem_crd(mword_t base, mword_t order, Rights rights = Rights())
			: Crd(base, order)
			{
				_rights(rights);
				_assign<TYPE_MASK, TYPE_SHIFT>(MEM_CRD_TYPE);
			}

			Rights rights() const
			{
				return Rights(_query<READ_MASK,  READ_SHIFT>(),
				              _query<WRITE_MASK, WRITE_SHIFT>(),
				              _query<EXEC_MASK,  EXEC_SHIFT>());
			}
	};


	/**
	 * I/O-capability-range descriptor
	 */
	class Io_crd : public Crd
	{
		public:

			Io_crd(mword_t base, mword_t order)
			: Crd(base, order)
			{
				_assign<TYPE_MASK, TYPE_SHIFT>(IO_CRD_TYPE);
				_assign<RIGHTS_MASK, RIGHTS_SHIFT>(RIGHTS_ALL);
			}
	};


	class Obj_crd : public Crd
	{
		public:

			enum {
				RIGHT_EC_RECALL = 0x1U,
			};

			Obj_crd() : Crd(0, 0)
			{
				_assign<TYPE_MASK, TYPE_SHIFT>(NULL_CRD_TYPE);
			}
	
			Obj_crd(mword_t base, mword_t order,
			        mword_t rights = RIGHTS_ALL)
			: Crd(base, order)
			{
				_assign<TYPE_MASK, TYPE_SHIFT>(OBJ_CRD_TYPE);
				_assign<RIGHTS_MASK, RIGHTS_SHIFT>(rights);
			}
	};


	/**
	 * Quantum-priority descriptor
	 */
	class Qpd : public Descriptor
	{
		private:

			enum {
				PRIORITY_MASK = 0xff,    PRIORITY_SHIFT =  0,
				QUANTUM_SHIFT = 12,
				QUANTUM_MASK  = (~0UL) >> QUANTUM_SHIFT
			};

			void _quantum(mword_t quantum)
			{ _assign<QUANTUM_MASK, QUANTUM_SHIFT>(quantum); }

			void _priority(mword_t priority)
			{ _assign<PRIORITY_MASK, PRIORITY_SHIFT>(priority); }

		public:

			enum { DEFAULT_QUANTUM = 10000, DEFAULT_PRIORITY = 1 };

			Qpd(mword_t quantum  = DEFAULT_QUANTUM,
			    mword_t priority = DEFAULT_PRIORITY)
			{
				_value = 0;
				_quantum(quantum), _priority(priority);
			}

			mword_t quantum()  const { return _query<QUANTUM_MASK,  QUANTUM_SHIFT>(); }
			mword_t priority() const { return _query<PRIORITY_MASK, PRIORITY_SHIFT>(); }
	};


	/**
	 * User-level thread-control block
	 */
	struct Utcb
	{
		mword_t  items;     /* number of untyped items uses lowest 16 bit, number of typed items uses bit 16-31, bit 32+ are ignored on 64bit */
		Crd      crd_xlt;   /* receive capability-range descriptor for translation */
		Crd      crd_rcv;   /* receive capability-range descriptor for delegation */
		mword_t tls;

		/**
		 * Data area
		 *
		 * The UTCB entries following the header hold message payload (normal
		 * IDC operations) or architectural state (exception handling).
		 */
		union {

			/* message payload */
			mword_t msg[];

			/* exception state */
			struct {
				mword_t mtd, instr_len, eip, eflags;
				unsigned misc[4];
				mword_t eax, ecx, edx, ebx;
				mword_t esp, ebp, esi, edi;
#ifdef __x86_64__
				mword_t rxx[8];
#endif
				unsigned long long qual[2];  /* exit qualification */
				unsigned ctrl[2];
				unsigned long long tsc;
				mword_t cr0, cr2, cr3, cr4;
//				unsigned misc3[44];
			};
		};

		struct Item {
			mword_t crd;
			mword_t hotspot;
			bool is_del() { return hotspot & 0x1; }
		};

		/**
		 * Set number of untyped message words
		 *
		 * Calling this function has the side effect of removing all typed
		 * message items from the message buffer.
		 */
		void set_msg_word(unsigned num) { items = num; }

		/**
		 * Return current number of message word in UTCB
		 */
		unsigned msg_words() { return items & 0xffffU; }

		/**
		 * Append message-transfer item to message buffer
		 *
		 * \param exception  true to append the item to an exception reply
		 */
		__attribute__((warn_unused_result))
		bool append_item(Crd crd, mword_t sel_hotspot,
		                 bool kern_pd = false,
		                 bool update_guest_pt = false,
		                 bool translate_map = false)
		{
			/* transfer items start at the end of the UTCB */
			items += 1 << 16;
			Item *item = reinterpret_cast<Item *>(this) + (PAGE_SIZE_BYTE / sizeof(struct Item)) - (items >> 16);

			/* check that there is enough space left on UTCB */
			if (msg + msg_words() >= reinterpret_cast<mword_t *>(item)) {
				items -= 1 << 16;
				return false;
			}

			/* map from hypervisor or current pd */
			unsigned h = kern_pd ? (1 << 11) : 0;

			/* update guest page table */
			unsigned g = update_guest_pt ? (1 << 10) : 0;

			item->hotspot = crd.hotspot(sel_hotspot) | g | h | (translate_map ? 2 : 1);
			item->crd = crd.value();

			return true;
		}

		/**
		 * Return typed item at postion i in UTCB
		 *
		 * \param i position of item requested, starts with 0
		 */
		Item * get_item(const unsigned i) {
			if (i > (PAGE_SIZE_BYTE / sizeof(struct Item))) return 0;
			Item * item = reinterpret_cast<Item *>(this) + (PAGE_SIZE_BYTE / sizeof(struct Item)) - i - 1;
			if (reinterpret_cast<mword_t *>(item) < this->msg) return 0;
			return item;
		}

		mword_t mtd_value() const { return static_cast<Mtd>(mtd).value(); }
	} __attribute__((packed));

	/**
	 * Size of event-specific portal window mapped at PD creation time
	 */
	enum {
		NUM_INITIAL_PT_LOG2 = 5,
		NUM_INITIAL_PT = 1 << NUM_INITIAL_PT_LOG2
	};

	/**
	 * Event-specific capability selectors
	 */
	enum {
		PT_SEL_PAGE_FAULT = 0xe,
		PT_SEL_PARENT     = 0x1a,  /* convention on Genode */
		PT_SEL_STARTUP    = 0x1e,
		PD_SEL            = 0x1b,
		PD_SEL_CAP_LOCK   = 0x1c,  /* convention on Genode */
		SM_SEL_EC_MAIN    = 0x1c,  /* convention on Genode */
		SM_SEL_EC         = 0x1d,  /* convention on Genode */
	};

}
#endif /* _PLATFORM__NOVA_SYSCALLS_GENERIC_H_ */
