/*
 * \brief  Syscall bindings for the NOVA microhypervisor
 * \author Norman Feske
 * \author Sebastian Sumpf
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

#ifndef _PLATFORM__NOVA_SYSCALLS_H_
#define _PLATFORM__NOVA_SYSCALLS_H_

#include <nova/stdint.h>

#include <base/printf.h>

#define ALWAYS_INLINE __attribute__((always_inline))

namespace Nova {

	enum {
		PAGE_SIZE_LOG2 = 12,
		PAGE_SIZE = 1 << PAGE_SIZE_LOG2,
		PAGE_MASK = ~(PAGE_SIZE - 1)
	};

	/**
	 * NOVA sytem-call IDs
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
	};


	class Descriptor
	{
		protected:

			unsigned _value;

			/**
			 * Assign bitfield to descriptor
			 */
			template<int MASK, int SHIFT>
			void _assign(unsigned new_bits)
			{
				_value &= ~(MASK << SHIFT);
				_value |= (new_bits & MASK) << SHIFT;
			}

			/**
			 * Query bitfield from descriptor
			 */
			template<int MASK, int SHIFT>
			unsigned _query() const { return (_value >> SHIFT) & MASK; }

		public:

			unsigned value() const { return _value; }
	};


	/**
	 * Message-transfer descriptor
	 */
	class Mtd
	{
		private:

			unsigned const _value;

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

			Mtd(unsigned value) : _value(value) { }

			unsigned value() const { return _value; }
	};


	class Crd : public Descriptor
	{
		protected:

			/**
			 * Bitfield holding the descriptor type
			 */
			enum {
				TYPE_MASK   = 0x3,     TYPE_SHIFT  =  0,
				BASE_MASK   = 0xfffff, BASE_SHIFT  = 12,
				ORDER_MASK  = 0x1f,    ORDER_SHIFT =  7,
				RIGHTS_MASK = 0x7c
			};

			/**
			 * Capability-range-descriptor types
			 */
			enum {
				NULL_CRD_TYPE   = 0,
				MEM_CRD_TYPE    = 1,
				IO_CRD_TYPE     = 2,
				OBJ_CRD_TYPE    = 3,
				RIGHTS_ALL      = 0x7c,
				IO_CRD_ALL      = IO_CRD_TYPE | RIGHTS_ALL,
				OBJ_CRD_ALL     = OBJ_CRD_TYPE | RIGHTS_ALL,
			};

			void _base(unsigned base)
			{ _assign<BASE_MASK, BASE_SHIFT>(base); }

			void _order(unsigned order)
			{ _assign<ORDER_MASK, ORDER_SHIFT>(order); }

		public:

			Crd(unsigned base, unsigned order) {
				_value = 0; _base(base), _order(order); }

			Crd(unsigned value) { _value = value; }

			unsigned hotspot(unsigned sel_hotspot) const
			{
				if ((value() & TYPE_MASK) == MEM_CRD_TYPE)
					return sel_hotspot & PAGE_MASK;

				return sel_hotspot << 12;
			}

			unsigned base()  const { return _query<BASE_MASK, BASE_SHIFT>(); }
			unsigned order() const { return _query<ORDER_MASK, ORDER_SHIFT>(); }
			bool is_null()   const { return (_value & TYPE_MASK) == NULL_CRD_TYPE; }
	};


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

			Mem_crd(unsigned base, unsigned order, Rights rights = Rights())
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

			Io_crd(unsigned base, unsigned order)
			: Crd(base, order)
			{
				_assign<TYPE_MASK | RIGHTS_MASK, TYPE_SHIFT>(IO_CRD_ALL);
			}
	};


	class Obj_crd : public Crd
	{
		public:

			Obj_crd(unsigned base, unsigned order)
			: Crd(base, order)
			{
				_assign<TYPE_MASK | RIGHTS_MASK, TYPE_SHIFT>(OBJ_CRD_ALL);
			}
	};


	/**
	 * Quantum-priority descriptor
	 */
	class Qpd : public Descriptor
	{
		private:

			enum {
				QUANTUM_MASK  = 0xfffff, QUANTUM_SHIFT  = 12,
				PRIORITY_MASK = 0xff,    PRIORITY_SHIFT =  0
			};

			void _quantum(unsigned quantum)
			{ _assign<QUANTUM_MASK, QUANTUM_SHIFT>(quantum); }

			void _priority(unsigned priority)
			{ _assign<PRIORITY_MASK, PRIORITY_SHIFT>(priority); }

		public:

			enum { DEFAULT_QUANTUM = 10000, DEFAULT_PRIORITY = 1 };

			Qpd(unsigned quantum  = DEFAULT_QUANTUM,
			    unsigned priority = DEFAULT_PRIORITY)
			{
				_value = 0;
				_quantum(quantum), _priority(priority);
			}

			unsigned quantum()  const { return _query<QUANTUM_MASK,  QUANTUM_SHIFT>(); }
			unsigned priority() const { return _query<PRIORITY_MASK, PRIORITY_SHIFT>(); }
	};


	/**
	 * User-level thread-control block
	 */
	struct Utcb
	{
		unsigned short ui;  /* number of untyped items */
		unsigned short ti;  /* number of typed itmes */
		Crd      crd_xlt;   /* receive capability-range descriptor for translation */
		Crd      crd_rcv;   /* receive capability-range descriptor for delegation */
		unsigned tls;

		/**
		 * Data area
		 *
		 * The UTCB entries following the header hold message payload (normal
		 * IDC operations) or architectural state (exception handling).
		 */
		union {

			/* message payload */
			unsigned msg[];

			/* exception state */
			struct {
				unsigned mtd, instr_len, eip, eflags;
				unsigned misc[4];
				unsigned eax, ecx, edx, ebx;
				unsigned esp, ebp, esi, edi;
				long long qual[2];  /* exit qualification */
				unsigned misc2[4];
				unsigned cr0, cr2, cr3, cr4;
				unsigned misc3[44];
			};
		};

		struct Item {
			unsigned crd;
			unsigned hotspot;
		};

		/**
		 * Set number of untyped message words
		 *
		 * Calling this function has the side effect of removing all typed
		 * message items from the message buffer.
		 */
		void set_msg_word(unsigned num) { ui = num; ti = 0; }

		/**
		 * Return current number of message word in UTCB
		 */
		unsigned msg_words() { return ui; }

		/**
		 * Append message-transfer item to message buffer
		 *
		 * \param exception  true to append the item to an exception reply
		 */
		void append_item(Crd crd, unsigned sel_hotspot,
		                 bool kern_pd = false,
		                 bool update_guest_pt = false)
		{
			/* transfer items start at the end of the UTCB */
			Item *item = reinterpret_cast<Item *>(this) + (PAGE_SIZE / sizeof(struct Item)) - ++ti;

			/* map from hypervisor or current pd */
			unsigned h = kern_pd ? (1 << 11) : 0;

			/* update guest page table */
			unsigned g = update_guest_pt ? (1 << 10) : 0;

			item->hotspot = crd.hotspot(sel_hotspot) | g | h | 1;
			item->crd = crd.value();

		}

		unsigned mtd_value() const { return static_cast<Mtd>(mtd).value(); }
	};

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
	};


	ALWAYS_INLINE
	inline unsigned eax(Syscall s, uint8_t flags, unsigned sel)
	{
		return sel << 8 | (flags & 0xf) << 4 | s;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_0(Syscall s, uint8_t flags, unsigned sel = 0)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              :
		              : "ecx", "edx", "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_1(Syscall s, uint8_t flags, mword_t p1)
	{
		mword_t status = eax(s, flags, 0);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              : "D" (p1)
		              : "ecx", "edx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_2(Syscall s, uint8_t flags, unsigned sel, mword_t p1, mword_t p2)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status)
		              : "D" (p1), "S" (p2)
		              : "ecx", "edx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_3(Syscall s, uint8_t flags, unsigned sel,
	                         mword_t p1, mword_t p2, mword_t p3)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  push %%ebx;"
		              "  mov  %%edx, %%ebx;"
		              "  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              " pop %%ebx;"
		              : "+a" (status)
		              : "D" (p1), "S" (p2), "d" (p3)
		              : "ecx");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t syscall_4(Syscall s, uint8_t flags, unsigned sel,
	                         mword_t p1, mword_t p2, mword_t p3, mword_t p4)
	{
		mword_t status = eax(s, flags, sel);

		asm volatile ("  push %%ebp;"
		              "  push %%ebx;"

		              "  mov %%ecx, %%ebx;"
		              "  mov %%esp, %%ecx;"
		              "  mov %%edx, %%ebp;"

		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "sysenter;"
		              "1:"

		              "  pop %%ebx;"
		              "  pop %%ebp;"
		              : "+a" (status)
		              : "D" (p1), "S" (p2), "c" (p3), "d" (p4)
		              : "memory");
		return status;
	}


	ALWAYS_INLINE
	inline uint8_t call(unsigned pt)
	{
		return syscall_0(NOVA_CALL, 0, pt);
	}


	ALWAYS_INLINE
	inline void reply(void *next_sp)
	{
		asm volatile ("sysenter;"
		              :
		              : "a" (NOVA_REPLY), "c" (next_sp)
		              : "memory");
	}


	ALWAYS_INLINE
	inline uint8_t create_pd(unsigned pd0, unsigned pd, Crd crd)
	{
		return syscall_2(NOVA_CREATE_PD, 0, pd0, pd, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t create_ec(unsigned ec, unsigned pd,
	                         mword_t cpu, mword_t utcb,
	                         mword_t esp, mword_t evt,
	                         bool global = 0)
	{
		return syscall_4(NOVA_CREATE_EC, global, ec, pd,
		                 (cpu & 0xfff) | (utcb & ~0xfff),
		                 esp, evt);
	}


	ALWAYS_INLINE
	inline uint8_t ec_ctrl(unsigned ec)
	{
		return syscall_1(NOVA_EC_CTRL, 0, ec);
	}

	ALWAYS_INLINE
	inline uint8_t create_sc(unsigned sc, unsigned pd, unsigned ec, Qpd qpd)
	{
		return syscall_3(NOVA_CREATE_SC, 0, sc, pd, ec, qpd.value());
	}


	ALWAYS_INLINE
	inline uint8_t create_pt(unsigned pt, unsigned pd, unsigned ec, Mtd mtd, mword_t eip)
	{
		return syscall_4(NOVA_CREATE_PT, 0, pt, pd, ec, mtd.value(), eip);
	}


	ALWAYS_INLINE
	inline uint8_t create_sm(unsigned sm, unsigned pd, mword_t cnt)
	{
		return syscall_2(NOVA_CREATE_SM, 0, sm, pd, cnt);
	}


	ALWAYS_INLINE
	inline uint8_t revoke(Crd crd, bool self = true)
	{
		return syscall_1(NOVA_REVOKE, self, crd.value());
	}


	ALWAYS_INLINE
	inline uint8_t lookup(Crd *crd)
	{
		mword_t status = eax(NOVA_LOOKUP, 0, 0);
		mword_t raw = crd->value();

		asm volatile ("  mov %%esp, %%ecx;"
		              "  call 0f;"
		              "0:"
		              "  addl $(1f-0b), (%%esp);"
		              "  mov (%%esp), %%edx;"
		              "  sysenter;"
		              "1:"
		              : "+a" (status), "+D" (raw)
		              :
		              : "ecx", "edx", "memory");

		*crd = Crd(raw);
		return status;
	}

	/**
	 * Semaphore operations
	 */
	enum Sem_op { SEMAPHORE_UP = 0, SEMAPHORE_DOWN = 1 };


	ALWAYS_INLINE
	inline uint8_t sm_ctrl(unsigned sm, Sem_op op)
	{
		return syscall_0(NOVA_SM_CTRL, op, sm);
	}


	ALWAYS_INLINE
	inline uint8_t assign_gsi(unsigned sm, mword_t dev, mword_t cpu)
	{
		return syscall_2(NOVA_ASSIGN_GSI, 0, sm, dev, cpu);
	}
}
#endif /* _PLATFORM__NOVA_SYSCALLS_H_ */
