/*
 * \brief  Syscall bindings for the NOVA microhypervisor
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \author Benjamin Lamowski
 * \date   2009-12-27
 */

/*
 * Copyright (c) 2009-2023 Genode Labs
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

#ifndef _INCLUDE__NOVA__SYSCALL_GENERIC_H_
#define _INCLUDE__NOVA__SYSCALL_GENERIC_H_

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
		NOVA_MISC       = 0x8, /* lookup, delegate, acpi_suspend */
		NOVA_EC_CTRL    = 0x9,
		NOVA_SC_CTRL    = 0xa,
		NOVA_PT_CTRL    = 0xb,
		NOVA_SM_CTRL    = 0xc,
		NOVA_ASSIGN_PCI = 0xd,
		NOVA_ASSIGN_GSI = 0xe,
		NOVA_PD_CTRL    = 0xf,
	};

	/**
	 * NOVA status codes returned by system-calls
	 */
	enum Status
	{
		NOVA_OK             = 0,
		NOVA_TIMEOUT        = 1,
		NOVA_IPC_ABORT      = 2,
		NOVA_INV_HYPERCALL  = 3,
		NOVA_INV_SELECTOR   = 4,
		NOVA_INV_PARAMETER  = 5,
		NOVA_INV_FEATURE    = 6,
		NOVA_INV_CPU        = 7,
		NOVA_INVD_DEVICE_ID = 8,
		NOVA_PD_OOM         = 9,
	};

	/**
	 * Hypervisor information page
	 */
	struct Hip
	{
		struct Mem_desc
		{
			enum Type {
				EFI_SYSTEM_TABLE    = -7,
				HYPERVISOR_LOG      = -6,
				FRAMEBUFFER         = -5,
				ACPI_XSDT           = -4,
				ACPI_RSDT           = -3,
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

		bool has_feature_iommu() const { return feature_flags & (1 << 0); }
		bool has_feature_vmx()   const { return feature_flags & (1 << 1); }
		bool has_feature_svm()   const { return feature_flags & (1 << 2); }

		struct Cpu_desc {
			uint8_t flags;
			uint8_t thread;
			uint8_t core;
			uint8_t package;
			uint8_t acpi_id;
			uint8_t family;
			uint8_t model;
			uint8_t stepping:4;
			uint8_t platform:3;
			uint8_t reserved:1;
			uint32_t patch;

			bool p_core() const { return flags & 0x2; }
			bool e_core() const { return flags & 0x4; }
		} __attribute__((packed));

		unsigned cpu_max() const {
			return (mem_desc_offset - cpu_desc_offset) / cpu_desc_size; }

		unsigned cpus() const {
			unsigned cpu_num = 0;

			for (unsigned i = 0; i < cpu_max(); i++)
				if (is_cpu_enabled(i))
					cpu_num++;

			return cpu_num;
		}

		Cpu_desc const * cpu_desc_of_cpu(unsigned i) const {
			if (i >= cpu_max())
				return nullptr;

			unsigned long desc_addr = reinterpret_cast<unsigned long>(this) +
			                          cpu_desc_offset + i * cpu_desc_size;
			return reinterpret_cast<Cpu_desc const *>(desc_addr);
		}

		bool is_cpu_enabled(unsigned i) const {
			Cpu_desc const * const desc = cpu_desc_of_cpu(i);
			return desc ? desc->flags & 0x1 : false;
		}

		/**
		 * Resort CPU ids such, that
		 * - the boot CPU id is ever logical CPU id 0
		 * - SMT threads of one CPU have logical CPU ids close together
		 * - P-Core has a smaller logical CPU id than E-Core CPUs
		 *
		 * Returns true, if re-mapping succeeded otherwise false.
		 *
		 * In case of failure, map_cpus will contain a 1:1 fallback mapping
		 * without any sorting as mentioned above.
		 */
		bool remap_cpu_ids(uint16_t *map_cpus, unsigned const max_cpus,
		                   unsigned const boot_cpu) const
		{
			unsigned const num_cpus = cpus();
			bool too_many_cpus = false;
			unsigned cpu_i = 0;

			/* fallback lambda in case re-ordering fails */
			auto remap_failure = [&] {
				for (uint16_t i = 0; i < max_cpus; i++) { map_cpus[i] = i; }
				return false;
			};

			/* assign boot cpu ever the virtual cpu id 0 */
			Cpu_desc const * const boot = cpu_desc_of_cpu(boot_cpu);
			if (!boot)
				return remap_failure();

			map_cpus[cpu_i++] = (uint8_t)boot_cpu;
			if (cpu_i >= num_cpus)
				return true;
			if (cpu_i >= max_cpus)
				return remap_failure();

			/* assign cores + SMT threads first and skip E-cores */
			bool done = for_all_cpus([&](auto const &cpu, auto const kernel_cpu_id) {
				if (kernel_cpu_id == boot_cpu)
					return false;

				/* handle normal or P-core */
				if (cpu.e_core())
					return false;

				map_cpus[cpu_i++] = (uint8_t)kernel_cpu_id;

				too_many_cpus = !!(cpu_i >= max_cpus);

				return (cpu_i >= num_cpus || too_many_cpus);
			});

			if (done)
				return too_many_cpus ? remap_failure() : true;

			/* assign remaining E-cores */
			done = for_all_cpus([&](auto &cpu, auto &kernel_cpu_id) {
				if (kernel_cpu_id == boot_cpu)
					return false;

				/* handle solely E-core */
				if (!cpu.e_core())
					return false;

				map_cpus[cpu_i++] = (uint16_t)kernel_cpu_id;

				too_many_cpus = !!(cpu_i >= max_cpus);

				return (cpu_i >= num_cpus || too_many_cpus);
			});

			return too_many_cpus ? remap_failure() : done;
		}

		/**
		 * Iterate over all CPUs in a _ever_ _consistent_ order.
		 */
		bool for_all_cpus(auto const &fn) const
		{
			for (uint16_t package = 0; package <= 255; package++) {
				for (uint16_t core = 0; core <= 255; core++) {
					for (uint16_t thread = 0; thread <= 255; thread++) {
						for (unsigned i = 0; i < cpu_max(); i++) {
							if (!is_cpu_enabled(i))
								continue;

							auto const cpu = cpu_desc_of_cpu(i);
							if (!cpu)
								continue;

							if (!(cpu->package == package && cpu->core == core &&
							      cpu->thread == thread))
								continue;

							bool done = fn(*cpu, i);
							if (done)
								return done;
						}
					}
				}
			}
			return false;
		}

		void for_each_enabled_cpu(auto const &fn) const
		{
			for (unsigned i = 0; i < cpu_max(); i++) {
				Cpu_desc const * cpu = cpu_desc_of_cpu(i);
				if (!is_cpu_enabled(i)) continue;
				if (!cpu) return;
				fn(*cpu, i);
			}
		}

	} __attribute__((packed));


	/**
	 * Semaphore operations
	 */
	enum Sem_op { SEMAPHORE_UP = 0U, SEMAPHORE_DOWN = 1U, SEMAPHORE_DOWNZERO = 0x3U };

	/**
	 * Ec operations
	 */
	enum Ec_op {
		EC_RECALL = 0U,
		EC_YIELD  = 1U,
		EC_DONATE_SC = 2U,
		EC_RESCHEDULE = 3U,
		EC_MIGRATE = 4U,
		EC_TIME = 5U,
		EC_GET_VCPU_STATE = 6U,
		EC_SET_VCPU_STATE = 7U,
		EC_MSR_ACCESS = 8U
	};

	enum Sc_op {
		SC_TIME_IDLE   = 0,
		SC_TIME_CROSS  = 1,
		SC_TIME_KILLED = 2,
		SC_EC_TIME     = 3,
	};

	/**
	 * Pd operations
	 */
	enum Pd_op { TRANSFER_QUOTA = 0U, PD_DEBUG = 2U };

	class Gsi_flags
	{
		private:

			uint8_t _value { 0 };

		public:

			enum Mode { HIGH, LOW, EDGE };

			Gsi_flags() { }

			Gsi_flags(Mode m)
			{
				switch (m) {
				case HIGH: _value = 0b110; break; /* level-high */
				case LOW:  _value = 0b111; break; /* level-low */
				case EDGE: _value = 0b100; break; /* edge-triggered */
				}
			}

			uint8_t value() const { return _value; }
	};


	class Descriptor
	{
		protected:

			mword_t _value { 0 };

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
				ACDB           = 1U << 0,   /* eax, ecx, edx, ebx */
				EBSD           = 1U << 1,   /* ebp, esi, edi */
				ESP            = 1U << 2,
				EIP            = 1U << 3,
				EFL            = 1U << 4,   /* eflags */
				ESDS           = 1U << 5,
				FSGS           = 1U << 6,
				CSSS           = 1U << 7,
				TR             = 1U << 8,
				LDTR           = 1U << 9,
				GDTR           = 1U << 10,
				IDTR           = 1U << 11,
				CR             = 1U << 12,
				DR             = 1U << 13,  /* DR7 */
				SYS            = 1U << 14,  /* Sysenter MSRs CS, ESP, EIP */
				QUAL           = 1U << 15,  /* exit qualification */
				CTRL           = 1U << 16,  /* execution controls */
				INJ            = 1U << 17,  /* injection info */
				STA            = 1U << 18,  /* interruptibility state */
				TSC            = 1U << 19,  /* time-stamp counter */
				EFER           = 1U << 20,  /* EFER MSR */
				PDPTE          = 1U << 21,  /* PDPTE0 .. PDPTE3 */
				R8_R15         = 1U << 22,  /* R8 .. R15 */
				SYSCALL_SWAPGS = 1U << 23,  /* SYSCALL and SWAPGS MSRs */
				TPR            = 1U << 24,  /* TPR and TPR threshold */
				TSC_AUX        = 1U << 25,  /* IA32_TSC_AUX used by rdtscp */
				XSAVE          = 1U << 26,  /* XCR and XSS used with XSAVE */
				FPU            = 1U << 31,  /* FPU state */

				IRQ   = EFL | STA | INJ | TSC,
				ALL   = (0x000fffff & ~CTRL) | EFER | R8_R15 | SYSCALL_SWAPGS | TPR,
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

			mword_t addr()   const { return base() << BASE_SHIFT; }
			mword_t base()   const { return _query<BASE_MASK, BASE_SHIFT>(); }
			mword_t order()  const { return _query<ORDER_MASK, ORDER_SHIFT>(); }
			bool is_null()   const { return (_value & TYPE_MASK) == NULL_CRD_TYPE; }
			uint8_t type()   const { return (uint8_t)_query<TYPE_MASK, TYPE_SHIFT>(); }
			uint8_t rights() const { return (uint8_t)_query<RIGHTS_MASK, RIGHTS_SHIFT>(); }
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
				RIGHT_PT_CALL   = 0x2U,
				RIGHT_PT_CTRL   = 0x1U,
				RIGHT_PT_XCPU   = 0x10U,
				RIGHT_SM_UP     = 0x1U,
				RIGHT_SM_DOWN   = 0x2U
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

			enum { DEFAULT_QUANTUM = 10000, DEFAULT_PRIORITY = 64 };

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
		/**
		 * Return physical size of UTCB in bytes
		 */
		static constexpr mword_t size() { return 4096; }

		/**
		 * Number of untyped items uses lowest 16 bit, number of typed items
		 * uses bit 16-31, bit 32+ are ignored on 64bit
		 */
		mword_t items;
		Crd     crd_xlt;   /* receive capability-range descriptor for translation */
		Crd     crd_rcv;   /* receive capability-range descriptor for delegation */
		mword_t tls;

		/**
		 * Data area
		 *
		 * The UTCB entries following the header hold message payload (normal
		 * IDC operations) or architectural state (exception handling).
		 */
		union {

			/* exception state */
			struct {
				mword_t mtd, instr_len, ip, flags;
				unsigned intr_state, actv_state, inj_info, inj_error;
				mword_t ax, cx, dx, bx;
				mword_t sp, bp, si, di;
#ifdef __x86_64__
				mword_t r8, r9, r10, r11, r12, r13, r14, r15;
#endif
				unsigned long long qual[2];  /* exit qualification */
				unsigned ctrl[2];
				mword_t cr0, cr2, cr3, cr4;
				unsigned long long xcr0, xss;
				mword_t pdpte[4];
#ifdef __x86_64__
				mword_t cr8, efer;
				unsigned long long star;
				unsigned long long lstar;
				unsigned long long cstar;
				unsigned long long fmask;
				unsigned long long kernel_gs_base;
				unsigned tpr;
				unsigned tpr_threshold;
#endif
				mword_t dr7, sysenter_cs, sysenter_sp, sysenter_ip;

				struct {
					unsigned short sel, ar;
					unsigned limit;
					mword_t  base;
#ifndef __x86_64__
					mword_t  reserved;
#endif
				} es, cs, ss, ds, fs, gs, ldtr, tr;
				struct {
					unsigned reserved0;
					unsigned limit;
					mword_t  base;
#ifndef __x86_64__
					mword_t  reserved1;
#endif
				} gdtr, idtr;
				unsigned long long tsc_val, tsc_off, tsc_aux;
				unsigned long long exit_reason;
				uint8_t fpu[2560];
			} __attribute__((packed));
			mword_t mr[(4096 - 4 * sizeof(mword_t)) / sizeof(mword_t)];
		};

		/* message payload */
		mword_t * msg() { return mr; }

		struct Item {
			mword_t crd;
			mword_t hotspot;
			bool is_del() const { return hotspot & 0x1; }
		};

#ifdef __x86_64__
		uint64_t read_r8()             const { return r8; }
		uint64_t read_r9()             const { return r9; }
		uint64_t read_r10()            const { return r10; }
		uint64_t read_r11()            const { return r11; }
		uint64_t read_r12()            const { return r12; }
		uint64_t read_r13()            const { return r13; }
		uint64_t read_r14()            const { return r14; }
		uint64_t read_r15()            const { return r15; }
		mword_t  read_efer()           const { return efer; }
		uint64_t read_star()           const { return star; }
		uint64_t read_lstar()          const { return lstar; }
		uint64_t read_cstar()          const { return cstar; }
		uint64_t read_fmask()          const { return fmask; }
		uint64_t read_kernel_gs_base() const { return kernel_gs_base; }
		uint32_t read_tpr()            const { return tpr; }
		uint32_t read_tpr_threshold()  const { return tpr_threshold; }

		void write_r8             (uint64_t value) { r8             = value; }
		void write_r9             (uint64_t value) { r9             = value; }
		void write_r10            (uint64_t value) { r10            = value; }
		void write_r11            (uint64_t value) { r11            = value; }
		void write_r12            (uint64_t value) { r12            = value; }
		void write_r13            (uint64_t value) { r13            = value; }
		void write_r14            (uint64_t value) { r14            = value; }
		void write_r15            (uint64_t value) { r15            = value; }
		void write_efer           (mword_t  value) { efer           = value; }
		void write_star           (uint64_t value) { star           = value; }
		void write_lstar          (uint64_t value) { lstar          = value; }
		void write_cstar          (uint64_t value) { cstar          = value; }
		void write_fmask          (uint64_t value) { fmask          = value; }
		void write_kernel_gs_base (uint64_t value) { kernel_gs_base = value; }
		void write_tpr            (uint32_t value) { tpr            = value; }
		void write_tpr_threshold  (uint32_t value) { tpr_threshold  = value; }
#else
		uint64_t read_r8()             const { return 0; }
		uint64_t read_r9()             const { return 0; }
		uint64_t read_r10()            const { return 0; }
		uint64_t read_r11()            const { return 0; }
		uint64_t read_r12()            const { return 0; }
		uint64_t read_r13()            const { return 0; }
		uint64_t read_r14()            const { return 0; }
		uint64_t read_r15()            const { return 0; }
		mword_t  read_efer()           const { return 0; }
		uint64_t read_star()           const { return 0; }
		uint64_t read_lstar()          const { return 0; }
		uint64_t read_cstar()          const { return 0; }
		uint64_t read_fmask()          const { return 0; }
		uint64_t read_kernel_gs_base() const { return 0; }
		uint32_t read_tpr()            const { return 0; }
		uint32_t read_tpr_threshold()  const { return 0; }

		void write_r8             (uint64_t) { }
		void write_r9             (uint64_t) { }
		void write_r10            (uint64_t) { }
		void write_r11            (uint64_t) { }
		void write_r12            (uint64_t) { }
		void write_r13            (uint64_t) { }
		void write_r14            (uint64_t) { }
		void write_r15            (uint64_t) { }
		void write_efer           (mword_t)  { }
		void write_star           (uint64_t) { }
		void write_lstar          (uint64_t) { }
		void write_cstar          (uint64_t) { }
		void write_fmask          (uint64_t) { }
		void write_kernel_gs_base (uint64_t) { }
		void write_tpr            (uint32_t) { }
		void write_tpr_threshold  (uint32_t) { }
#endif

		/**
		 * Set number of untyped message words
		 *
		 * Calling this function has the side effect of removing all typed
		 * message items from the message buffer.
		 */
		void set_msg_word(mword_t const num) { items = num; }

		/**
		 * Return current number of message word in UTCB
		 */
		unsigned msg_words() { return items & 0xffffU; }

		/**
		 * Return current number of message items on UTCB
		 */
		unsigned msg_items() { return (unsigned)(items >> 16); }

		/**
		 * Append message-transfer item to message buffer
		 *
		 * \param exception  true to append the item to an exception reply
		 */
		__attribute__((warn_unused_result))
		bool append_item(Crd crd, mword_t sel_hotspot,
		                 bool kern_pd = false,
		                 bool update_guest_pt = false,
		                 bool translate_map = false,
		                 bool dma_mem = false,
		                 bool write_combined = false)
		{
			/* transfer items start at the end of the UTCB */
			items += 1 << 16;
			Item *item = reinterpret_cast<Item *>(this);
			item += (PAGE_SIZE_BYTE / sizeof(struct Item)) - msg_items();

			/* check that there is enough space left on UTCB */
			if (msg() + msg_words() >= reinterpret_cast<mword_t *>(item)) {
				items -= 1 << 16;
				return false;
			}

			/* map from hypervisor or current pd */
			unsigned h = kern_pd ? (1 << 11) : 0;

			/* map write-combined */
			unsigned wc = write_combined ? (1 << 10) : 0;

			/* update guest page table */
			unsigned g = update_guest_pt ? (1 << 9) : 0;

			/* mark memory dma able */
			unsigned d = dma_mem ? (1 << 8) : 0;

			/* set type of delegation, either 'map' or 'translate and map' */
			unsigned m = translate_map ? 2 : 1;

			item->hotspot = crd.hotspot(sel_hotspot) | g | h | wc | d | m;
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
			if (reinterpret_cast<mword_t *>(item) < this->msg()) return 0;
			return item;
		}

		mword_t mtd_value() const { return static_cast<Mtd>(mtd).value(); }

		/**
		 * Return fault address and type of page-fault message
		 */
		mword_t pf_addr() const { return (mword_t)qual[1]; }
		uint8_t pf_type() const { return (uint8_t)qual[0]; }
	};

	static_assert(sizeof(Utcb) == 4096, "Unexpected size of UTCB");

	/**
	 * Size of event-specific portal window mapped at PD creation time
	 */
	enum {
		NUM_INITIAL_PT_LOG2 = 5,
		NUM_INITIAL_PT = 1UL << NUM_INITIAL_PT_LOG2,
		NUM_INITIAL_PT_RESERVED = 2 * NUM_INITIAL_PT,
		NUM_INITIAL_VCPU_PT_LOG2 = 8,
		NUM_INITIAL_VCPU_PT = 1UL << NUM_INITIAL_VCPU_PT_LOG2,
	};

	/**
	 * Event-specific capability selectors
	 */
	enum {
		PT_SEL_PAGE_FAULT = 0xe,
		PT_SEL_PARENT     = 0x1a,  /* convention on Genode */
		EC_SEL_THREAD     = 0x1c,  /* convention on Genode */
		PT_SEL_STARTUP    = 0x1e,
		SM_SEL_SIGNAL     = 0x1e,  /* alias of PT_SEL_STARTUP */
		PT_SEL_RECALL     = 0x1f,
		SM_SEL_EC         = 0x1d,  /* convention on Genode */
	};

}
#endif /* _INCLUDE__NOVA__SYSCALL_GENERIC_H_ */
