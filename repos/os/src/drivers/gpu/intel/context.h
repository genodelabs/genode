/*
 * \brief  Intel GPU logical ring context for Broadwell and newer
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOGICAL_RING_CONTEXT_H_
#define _LOGICAL_RING_CONTEXT_H_

/* Genode includes */
#include <util/mmio.h>
#include <util/string.h>

/* local includes */
#include <types.h>
#include <commands.h>
#include <utils.h>

namespace Igd {

	struct Context_status_qword;

	struct Common_context_regs;

	struct Generation;

	struct Hardware_status_page;
	class  Pphardware_status_page;
	template <addr_t RING_BASE> class Execlist_context;
	template <addr_t RING_BASE> class Ppgtt_context;
	class Engine_context;
	class Ext_engine_context;
	class Urb_atomic_context;

	class Rcs_context;
}

struct Igd::Generation { unsigned value; };

/*
 * IHD-OS-BDW-Vol 6-11.15 p. 8
 * IHD-OS-BDW-Vol 2d-11.15 p. 111
 */
struct Igd::Context_status_qword : Genode::Register<64>
{
	struct Context_id          : Bitfield<32, 32> { };
	/* only valid if Preempted set bit */
	struct Lite_restore        : Bitfield<15,  1> { };
	struct Display_plane       : Bitfield<12,  3>
	{
		enum {
			DISPLAY_PLANE_A        = 0b000,
			DISPLAY_PLANE_B        = 0b001,
			DISPLAY_PLANE_C        = 0b010,
			DISPLAY_PLANE_SPRITE_A = 0b011,
			DISPLAY_PLANE_SPRITE_B = 0b100,
			DISPLAY_PLANE_SPRITE_C = 0b101,
		};
	};
	/* only valid if Wait_on_semaphore bit set */
	struct Semaphore_wait_mode : Bitfield<11,  1>
	{
		enum {
			SIGNAL_MODE = 0b00,
			POLL_MODE   = 0b01,
		};
	};
	struct Wait_on_scanline  : Bitfield< 8,  1> { };
	struct Wait_on_semaphore : Bitfield< 7,  1> { };
	struct Wait_on_v_blank   : Bitfield< 6,  1> { };
	struct Wait_on_sync_flip : Bitfield< 5,  1> { };
	struct Context_complete  : Bitfield< 4,  1> { };
	struct Active_to_idle    : Bitfield< 3,  1> { };
	struct Element_switch    : Bitfield< 2,  1> { };
	struct Preempted         : Bitfield< 1,  1> { };
	struct Idle_to_active    : Bitfield< 0,  1> { };
};


struct Igd::Common_context_regs : public Genode::Mmio
{
	template <long int OFFSET>
	struct Common_register : Register<OFFSET * sizeof(uint32_t), 32> { };

	template <long int OFFSET, size_t NUM>
	struct Common_register_array : Register_array<OFFSET * sizeof(uint32_t), 32, NUM, 32> { };

	template <long int OFFSET, size_t NUM>
	struct Common_register_array_64 : Register_array<OFFSET * sizeof(uint32_t), 64, NUM, 64> { };

	addr_t _base;

	Common_context_regs(addr_t base) : Genode::Mmio(base), _base(base) { }

	addr_t base() const { return _base; }

	template <typename T>
	void write_offset(typename T::access_t const value)
	{
		write<T>(value + T::OFFSET);
	}
};


/*
 * IHD-OS-BDW-Vol 3-11.15 p. 18  (for VCS)
 * IHD-OS-BDW-Vol 3-11.15 p. 20  (for BCS)
 * IHD-OS-BDW-Vol 3-11.15 p. 22  (for VECS)
 * IHD-OS-BDW-Vol 7-11.15 p. 27  (for RCS)
 *
 * All engines use the same layout until offset 0x118.
 */
template <Genode::addr_t RING_BASE>
class Igd::Execlist_context : public Igd::Common_context_regs
{
	public:

		/* MI_NOOP */
		struct Noop_1 : Common_register<0x0000> { };

		/*
		 * IHD-OS-BDW-Vol 2a-11.15 p. 841 ff.
		 */
		struct Load_immediate_header : Common_register<0x0001> { };

		/*
		 * XXX see i915 intel_lrc.h
		 */
		struct Context_control_mmio : Common_register<0x0002>
		{
			enum { OFFSET = 0x244, };
		};
		struct Context_control_value : Common_register<0x0003>
		{
			using R = Common_register<0x0003>;

			struct Mask_bits : R::template Bitfield<16, 16> { };

			struct Inhibit_syn_context_switch_mask : R::template Bitfield<19, 1> { };
			struct Inhibit_syn_context_switch      : R::template Bitfield< 3, 1> { };

			struct Engine_context_save_inhibit_mask : R::template Bitfield< 18, 1> { };
			struct Engine_context_save_inhibit : R::template Bitfield< 2, 1> { };

			struct Rs_context_enable_mask : R::template Bitfield<17, 1> { };
			struct Rs_context_enable      : R::template Bitfield< 1, 1> { };

			struct Engine_context_restore_inhibit_mask : R::template Bitfield<16, 1> { };
			struct Engine_context_restore_inhibit      : R::template Bitfield< 0, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1350 ff
		 */
		struct Ring_buffer_head_mmio : Common_register<0x0004>
		{
			enum { OFFSET = 0x34, };
		};
		struct Ring_buffer_head_value : Common_register<0x0005>
		{
			using R = Common_register<0x0005>;

			struct Wrap_count   : R::template Bitfield<21,11> { };
			struct Head_offset  : R::template Bitfield< 2,19> { };
			struct Reserved_mbz : R::template Bitfield< 0, 2> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1353 ff
		 */
		struct Ring_buffer_tail_mmio : Common_register<0x0006>
		{
			enum { OFFSET = 0x30, };
		};
		struct Ring_buffer_tail_value : Common_register<0x0007>
		{
			using R = Common_register<0x0007>;

			struct Reserved_mbz_1 : R::template Bitfield<21, 11> { };
			struct Tail_offset    : R::template Bitfield< 3, 18> { };
			struct Reserved_mbz_2 : R::template Bitfield< 0,  3> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1352 ff
		 */
		struct Ring_buffer_start_mmio : Common_register<0x0008>
		{
			enum { OFFSET = 0x38, };
		};
		struct Ring_buffer_start_value : Common_register<0x0009>
		{
			using R = Common_register<0x0009>;

			struct Starting_address : R::template Bitfield<12, 20> { };
			struct Reserved_mbz     : R::template Bitfield< 0, 12> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1345 ff
		 */
		struct Ring_buffer_control_mmio : Common_register<0x000A>
		{
			enum { OFFSET = 0x3c, };
		};
		struct Ring_buffer_control_value : Common_register<0x000B>
		{
			using R = Common_register<0x000B>;

			struct Reserved_mbz_1 : R::template Bitfield<21, 11> { };
			struct Buffer_length  : R::template Bitfield<12,  9> { };
			struct RBwait         : R::template Bitfield<11,  1>
			{
				enum { CLEAR = 0b01, };
			};

			struct Semaphore_wait : R::template Bitfield<10,  1>
			{
				enum { CLEAR = 0b01, };
			};

			struct Reserved_mbz_2 : R::template Bitfield< 3,  7> { };
			struct Arhp           : R::template Bitfield< 1,  2>
			{
				enum {
					MI_AUTOREPORT_OFF   = 0,
					MI_AUTOREPORT_64KB  = 1,
					MI_AUTOREPORT_4KB   = 2,
					MI_AUTOREPORT_128KB = 3
				};
			};

			struct Ring_buffer_enable : R::template Bitfield< 0,  1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 142
		 */
		struct Bb_addr_udw_mmio : Common_register<0x000C>
		{
			enum { OFFSET = 0x168, };
		};
		struct Bb_addr_udw_value : Common_register<0x000D> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 129
		 */
		struct Bb_addr_mmio : Common_register<0x000E>
		{
			enum { OFFSET = 0x140, };
		};
		struct Bb_addr_value : Common_register<0x000F> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 138
		 */
		struct Bb_state_mmio : Common_register<0x0010>
		{
			enum { OFFSET = 0x110, };
		};
		struct Bb_state_value : Common_register<0x0011>
		{
			struct Address_space_indicator : Bitfield<5, 1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1402
		 */
		struct Sbb_addr_udw_mmio : Common_register<0x0012>
		{
			enum { OFFSET = 0x11C, };
		};
		struct Sbb_addr_udw_value : Common_register<0x0013> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1397
		 */
		struct Sbb_addr_mmio : Common_register<0x0014>
		{
			enum { OFFSET = 0x114, };
		};
		struct Sbb_addr_value : Common_register<0x0015> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1399
		 */
		struct Sbb_state_mmio : Common_register<0x0016>
		{
			enum { OFFSET = 0x118, };
		};
		struct Sbb_state_value : Common_register<0x0017> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 131 ff
		 *
		 * RCS only
		 */
		struct Bb_per_ctx_ptr_mmio : Common_register<0x0018>
		{
			enum { OFFSET = 0x1C0, };
		};
		struct Bb_per_ctx_ptr_value : Common_register<0x0019>
		{
			using R = Common_register<0x0019>;

			struct Address      : R::template Bitfield<12, 20> { };
			struct Reserved_mbz : R::template Bitfield< 2, 10> { };
			struct Enable       : R::template Bitfield< 1,  1> { };
			struct Valid        : R::template Bitfield< 0,  1> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 821 ff
		 *
		 * RCS only
		 */
		struct Rcs_indirect_ctx_mmio : Common_register<0x001A>
		{
			enum { OFFSET = 0x1C4, };
		};
		struct Rcs_indirect_ctx_value : Common_register<0x001B>
		{
			using R = Common_register<0x001B>;

			struct Address : R::template Bitfield< 6, 26> { };
			struct Size    : R::template Bitfield< 0,  6> { };
		};

		/*
		 * XXX
		 *
		 * RCS only
		 */
		struct Rcs_indirect_ctx_offset_mmio : Common_register<0x001C>
		{
			enum { OFFSET = 0x1C8, };
		};
		struct Rcs_indirect_ctx_offset_value : Common_register<0x001D>
		{
			using R = Common_register<0x001D>;

			struct Reserved_mbz_1 : R::template Bitfield<16, 16> { };
			struct Offset         : R::template Bitfield< 6, 10> { };
			struct Reserved_mbz_2 : R::template Bitfield< 0,  6> { };
		};

		/*
		 * XXX
		 *
		 * RCS only
		 */
		struct Rcs_noop_1 : Common_register_array<0x001E, 2> { };

	public:

		Execlist_context(addr_t const base) : Common_context_regs(base) { }

		void setup(addr_t     const ring_buffer_start,
		           size_t     const ring_buffer_length,
		           uint32_t   const immediate_header,
		           Generation const gen)
		{
			write<Load_immediate_header>(immediate_header);
			write_offset<Context_control_mmio>(RING_BASE);
			{
				typename Context_control_value::access_t v = read<Context_control_value>();
				Context_control_value::Engine_context_restore_inhibit_mask::set(v, 1);
				Context_control_value::Engine_context_restore_inhibit::set(v, 1);
				Context_control_value::Inhibit_syn_context_switch_mask::set(v, 1);
				Context_control_value::Inhibit_syn_context_switch::set(v, 1);
				if (gen.value < 11) {
					Context_control_value::Engine_context_save_inhibit_mask::set(v, 1);
					Context_control_value::Engine_context_save_inhibit::set(v, 0);
					Context_control_value::Rs_context_enable_mask::set(v, 1);
					Context_control_value::Rs_context_enable::set(v, 0);
				}
				write<Context_control_value>(v);
			}
			write_offset<Ring_buffer_head_mmio>(RING_BASE);
			write_offset<Ring_buffer_tail_mmio>(RING_BASE);
			write_offset<Ring_buffer_start_mmio>(RING_BASE);
			{
				typename Ring_buffer_start_value::access_t v = read<Ring_buffer_start_value>();
				/* shift ring_buffer_start value accordingly */
				typename Ring_buffer_start_value::access_t const addr =
					(uint32_t)Ring_buffer_start_value::Starting_address::get(ring_buffer_start);

				Ring_buffer_start_value::Starting_address::set(v, addr);
				write<Ring_buffer_start_value>(v);
			}
			write_offset<Ring_buffer_control_mmio>(RING_BASE);
			{
				typename Ring_buffer_control_value::access_t v = read<Ring_buffer_control_value>();
				/* length is given in number of pages */
				Ring_buffer_control_value::Buffer_length::set(v, (ring_buffer_length / PAGE_SIZE) - 1);
				/* according to the PRM it should be disable b/c of the amount of reports generated */
				Ring_buffer_control_value::Arhp::set(v, Ring_buffer_control_value::Arhp::MI_AUTOREPORT_OFF);
				Ring_buffer_control_value::Ring_buffer_enable::set(v, 1);
				write<Ring_buffer_control_value>(v);
			}
			write_offset<Bb_addr_udw_mmio>(RING_BASE);
			write_offset<Bb_addr_mmio>(RING_BASE);
			write_offset<Bb_state_mmio>(RING_BASE);
			{
				/* should actually not be written by software */
				typename Bb_state_value::access_t v = 0;
				Bb_state_value::Address_space_indicator::set(v, 1);
				write<Bb_state_value>(v);
			}
			write_offset<Sbb_addr_udw_mmio>(RING_BASE);
			write_offset<Sbb_addr_mmio>(RING_BASE);
			write_offset<Sbb_state_mmio>(RING_BASE);
		}

		size_t tail_offset()
		{
			return read<typename Ring_buffer_tail_value::Tail_offset>();
		}

		void tail_offset(size_t offset)
		{
			write<typename Ring_buffer_tail_value::Tail_offset>(offset);
		}

		size_t head_offset()
		{
			return read<typename Ring_buffer_head_value::Head_offset>();
		}

		/*********************
		 ** Debug interface **
		 *********************/

		void dump()
		{
			using namespace Genode;

			log("Execlist_context");
			log("  Load_immediate_header: ", Hex(read<Load_immediate_header>(), Hex::PREFIX, Hex::PAD));
			log("  Context_control:       ", Hex(read<Context_control_value>(), Hex::PREFIX, Hex::PAD));
			log("  Ring_buffer_head:      ", Hex(read<Ring_buffer_head_value>(), Hex::PREFIX, Hex::PAD));
			log("     Wrap_count:         ", Hex(read<typename Ring_buffer_head_value::Wrap_count>(), Hex::PREFIX, Hex::PAD));
			log("  Ring_buffer_tail:      ", Hex(read<Ring_buffer_tail_value>(), Hex::PREFIX, Hex::PAD));
			log("  Ring_buffer_start:     ", Hex(read<Ring_buffer_start_value>(), Hex::PREFIX, Hex::PAD));
			log("  Ring_buffer_control:   ", Hex(read<Ring_buffer_control_value>(), Hex::PREFIX, Hex::PAD));
			log("  Bb_addr_udw:           ", Hex(read<Bb_addr_udw_value>(), Hex::PREFIX, Hex::PAD));
			log("  Bb_addr:               ", Hex(read<Bb_addr_value>(), Hex::PREFIX, Hex::PAD));
			log("  Bb_state:              ", Hex(read<Bb_state_value>(), Hex::PREFIX, Hex::PAD));
			log("  Sbb_addr_udw:          ", Hex(read<Sbb_addr_udw_value>(), Hex::PREFIX, Hex::PAD));
			log("  Sbb_addr:              ", Hex(read<Sbb_addr_value>(), Hex::PREFIX, Hex::PAD));
			log("  Sbb_state:             ", Hex(read<Sbb_state_value>(), Hex::PREFIX, Hex::PAD));
		}
};


template <Genode::addr_t RING_BASE>
class Igd::Ppgtt_context : public Igd::Common_context_regs
{
	public:

		/* MI_NOOP */
		struct Noop_1 : Common_register<0x0020> { };

		/*
		 * IHD-OS-BDW-Vol 2a-11.15 p. 841 ff.
		 */
		struct Load_immediate_header : Common_register<0x0021> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 283
		 *
		 * RCS only
		 */
		struct Cs_ctx_timestamp_mmio : Common_register<0x0022>
		{
			enum { OFFSET = 0x3A8, };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1143
		 */
		struct Pdp_3_udw_mmio : Common_register<0x0024>
		{
			enum { OFFSET = 0x28C, };
		};
		struct Pdp_3_udw_value : Common_register<0x0025>
		{
			struct Value : Bitfield<0,32> { };
		};

		struct Pdp_3_ldw_mmio : Common_register<0x0026>
		{
			enum { OFFSET = 0x288, };
		};
		struct Pdp_3_ldw_value : Common_register<0x0027>
		{
			struct Value : Bitfield<0,32> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1142
		 */
		struct Pdp_2_udw_mmio : Common_register<0x0028>
		{
			enum { OFFSET = 0x284, };
		};
		struct Pdp_2_udw_value : Common_register<0x0029>
		{
			struct Value : Bitfield<0,32> { };
		};

		struct Pdp_2_ldw_mmio : Common_register<0x002A>
		{
			enum { OFFSET = 0x280, };
		};
		struct Pdp_2_ldw_value : Common_register<0x002B>
		{
			struct Value : Bitfield<0,32> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1141
		 */
		struct Pdp_1_udw_mmio : Common_register<0x002C>
		{
			enum { OFFSET = 0x27C, };
		};
		struct Pdp_1_udw_value : Common_register<0x002D>
		{
			struct Value : Bitfield<0,32> { };
		};

		struct Pdp_1_ldw_mmio : Common_register<0x002E>
		{
			enum { OFFSET = 0x278, };
		};
		struct Pdp_1_ldw_value : Common_register<0x002F>
		{
			struct Value : Bitfield<0,32> { };
		};

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1140
		 */
		struct Pdp_0_udw_mmio : Common_register<0x0030>
		{
			enum { OFFSET = 0x274, };
		};
		struct Pdp_0_udw_value : Common_register<0x0031>
		{
			struct Value : Bitfield<0,32> { };
		};

		struct Pdp_0_ldw_mmio : Common_register<0x0032>
		{
			enum { OFFSET = 0x270, };
		};
		struct Pdp_0_ldw_value : Common_register<0x0033>
		{
			struct Value : Bitfield<0,32> { };
		};

		struct Noop_2 : Common_register_array<0x0034, 12> { };
		struct Noop_3 : Common_register_array<0x0040,  1> { };

		struct Load_immediate_header_2 : Common_register<0x0041> { };

		/*
		 * IHD-OS-BDW-Vol 2c-11.15 p. 1325
		 */
		struct R_pwr_clk_state_mmio : Common_register<0x0042>
		{
			enum { OFFSET = 0x0C8, };
		};
		struct R_pwr_clk_state_value : Common_register<0x0043>
		{
			using R = Common_register<0x0043>;

			struct Power_clock_state_enable : R::template Bitfield<31,  1> { };
			struct Power_clock_state        : R::template Bitfield< 0, 31> { };
		};

		/*
		 * IHD-OS-BDW-Vol 7-11.15 p. 659
		 */
		struct Gpgpu_csr_base_Address : Common_register_array<0x00044, 3> { };

		struct Noop_4 : Common_register_array<0x0047, 9> { };

	public:

		Ppgtt_context(addr_t const base) : Common_context_regs(base) { }

		void setup(uint64_t const plm4_addr)
		{
			write<Load_immediate_header>(0x11001011);
			write_offset<Cs_ctx_timestamp_mmio>(RING_BASE);
			{
				write<Cs_ctx_timestamp_mmio>(0);
			}
			write_offset<Pdp_3_udw_mmio>(RING_BASE);
			write_offset<Pdp_3_ldw_mmio>(RING_BASE);
			write_offset<Pdp_2_udw_mmio>(RING_BASE);
			write_offset<Pdp_2_ldw_mmio>(RING_BASE);
			write_offset<Pdp_1_udw_mmio>(RING_BASE);
			write_offset<Pdp_1_ldw_mmio>(RING_BASE);
			write_offset<Pdp_0_udw_mmio>(RING_BASE);
			{
				Genode::uint32_t const udw = (Genode::uint32_t)((plm4_addr >> 16) >> 16);
				write<Pdp_0_udw_value>(udw);
			}
			write_offset<Pdp_0_ldw_mmio>(RING_BASE);
			{
				Genode::uint32_t const ldw = (Genode::uint32_t)plm4_addr;
				write<Pdp_0_ldw_value>(ldw);
			}

			write_offset<R_pwr_clk_state_mmio>(RING_BASE);
		}

		/*********************
		 ** Debug interface **
		 *********************/

		void dump()
		{
			using namespace Genode;

			using C = Ppgtt_context<RING_BASE>;

			log("Ppgtt_context");
			log("  Pdp_0_udw: ", Hex(read<C::Pdp_0_udw_value>(), Hex::PREFIX, Hex::PAD));
			log("  Pdp_0_ldw: ", Hex(read<C::Pdp_0_ldw_value>(), Hex::PREFIX, Hex::PAD));
		}
};


/*
 * TODO
 */
class Igd::Engine_context
{
	public:

		Engine_context() { }
};


/*
 * TODO
 */
class Igd::Ext_engine_context
{
	public:

		Ext_engine_context() { }
};


/*
 * TODO
 */
class Igd::Urb_atomic_context
{
	public:

		Urb_atomic_context() { }
};


/*
 * IHD-OS-BDW-Vol 2d-11.15 p. 199
 */
class Igd::Hardware_status_page : public Igd::Common_context_regs
{
	public:

		/*
		 * See ISR register definition
		 */
		struct Interrupt_status_register_storage : Common_register<0> { };

		/*
		 * See RING_BUFFER_HEAD_RCSUNIT definition
		 */
		struct Ring_head_ptr_storage : Common_register<4> { };

		/*
		 * dword 0x30 to 0x3ff is available for driver usage
		 */
		struct Sequence_number : Register<0x30*4, 64> { };
		struct Semaphore       : Register<0x32*4, 32> { };

		/*
		 * See CTXT_ST_BUF
		 */
		enum {
			CONTEXT_STATUS_DWORDS_NUM = 12,
			CONTEXT_STATUS_REGISTERS  = CONTEXT_STATUS_DWORDS_NUM / 2,
		};
		struct Context_status_dwords : Common_register_array_64<16, CONTEXT_STATUS_REGISTERS> { };
		struct Last_written_status_offset : Common_register<31> { };

		Hardware_status_page(addr_t base)
		: Common_context_regs(base)
		{
			semaphore(0);
		}

		void semaphore(uint32_t value)
		{
			write<Semaphore>(value);
			Igd::wmb();
		}

		uint64_t sequence_number()
		{
			/* invalidates cache before reading */
			Utils::clflush((void *)(base() + Sequence_number::OFFSET));
			return read<Sequence_number>();
		}

		void dump(bool raw = false)
		{
			using namespace Genode;

			if (raw) {
				uint32_t const *p = (uint32_t const *)base();
				for (uint32_t i = 0; i < PAGE_SIZE / sizeof(uint32_t); i += 8) {
					log(Hex(i, Hex::PREFIX, Hex::PAD), "  ",
					    Hex(p[i  ], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+1], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+2], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+3], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+4], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+5], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+6], Hex::PREFIX, Hex::PAD), " ",
					    Hex(p[i+7], Hex::PREFIX, Hex::PAD));
				}
			} else {
				log("Hardware_status_page");
				log("   Interrupt_status_register_storage: ",
				    Hex(read<Interrupt_status_register_storage>(),
				        Hex::PREFIX, Hex::PAD));
				log("   Ring_head_ptr_storage: ",
				    Hex(read<Ring_head_ptr_storage>(),
				        Hex::PREFIX, Hex::PAD));

				auto const cs_last = read<Last_written_status_offset>();

				log("   Last_written_status_offset: ",
				    Hex(cs_last, Hex::PREFIX, Hex::PAD));

				for (unsigned i = 0; i < CONTEXT_STATUS_REGISTERS; i++) {
					using C = Igd::Context_status_qword;
					C::access_t v = read<Context_status_dwords>(i);
					log("   Context_status ", i);
					log("    Context_id:          ", C::Context_id::get(v));
					log("    Lite_restore:        ", C::Lite_restore::get(v));
					log("    Display_plane:       ", C::Display_plane::get(v));
					log("    Semaphore_wait_mode: ", C::Semaphore_wait_mode::get(v));
					log("    Wait_on_scanline:    ", C::Wait_on_scanline::get(v));
					log("    Wait_on_semaphore:   ", C::Wait_on_semaphore::get(v));
					log("    Wait_on_v_blank:     ", C::Wait_on_v_blank::get(v));
					log("    Wait_on_sync_flip:   ", C::Wait_on_sync_flip::get(v));
					log("    Context_complete:    ", C::Context_complete::get(v));
					log("    Active_to_idle:      ", C::Active_to_idle::get(v));
					log("    Element_switch:      ", C::Element_switch::get(v));
					log("    Preempted:           ", C::Preempted::get(v));
					log("    Idle_to_active:      ", C::Idle_to_active::get(v));
				}
			}
		}
};


/*
 * IHD-OS-BDW-Vol 2d-11.15 p. 303
 */
class Igd::Pphardware_status_page : public Igd::Common_context_regs
{
	public:

		struct Ring_head_ptr_storage : Common_register<4> { };

		Pphardware_status_page(addr_t base)
		: Common_context_regs(base) { }
};


/*
 */
class Igd::Rcs_context
{
	public:

		enum {
			HW_ID         = 0,
			CONTEXT_PAGES = 22 /* ctx */ + 1 /* GuC */,
			RING_PAGES    = 4,

			RCS_RING_BASE = 0x2000,

			HW_STATUS_PAGE_SIZE = PAGE_SIZE,

			/*
			 * IHD-OS-BDW-Vol 7-11.15 p. 27 ff
			 */
			EXECLIST_CTX_START = 0x0000,
			EXECLIST_CTX_END   = 0x0020,
			EXECLIST_CTX_SIZE  = (EXECLIST_CTX_END - EXECLIST_CTX_START) * sizeof(uint32_t),
			EXECLIST_CTX_IH    = 0x1100101B,

			PPGTT_CTX_START = EXECLIST_CTX_END,
			PPGTT_CTX_END   = 0x0050,
			PPGTT_CTX_SIZE  = (PPGTT_CTX_END - PPGTT_CTX_START) * sizeof(uint32_t),
			PPGTT_CTX_IH    = 0x11001011,
			PPGTT_CTX_IH_2  = 0x11000001,

			ENGINE_CTX_START = PPGTT_CTX_END,
			ENGINE_CTX_END   = 0x0EC0,
			ENGINE_CTX_SIZE  = (ENGINE_CTX_END - ENGINE_CTX_START) * sizeof(uint32_t),

			EXT_ENGINE_CTX_START = ENGINE_CTX_END,
			EXT_ENGINE_CTX_END   = 0x26B0,

			URB_ATOMIC_STORE_START = EXT_ENGINE_CTX_END,
			URB_ATOMIC_STORE_END   = 0x46B0,
			URB_ATOMIC_STORE_SIZE  = (URB_ATOMIC_STORE_END - URB_ATOMIC_STORE_START) * sizeof(uint32_t),
		};


	private:

		Pphardware_status_page          _hw_status_page;
		Execlist_context<RCS_RING_BASE> _execlist_context;
		Ppgtt_context<RCS_RING_BASE>    _ppgtt_context;
		Engine_context                  _engine_context     { };
		Ext_engine_context              _ext_engine_context { };
		Urb_atomic_context              _urb_atomic_context { };

	public:

		Rcs_context(addr_t const map_base)
		:
			_hw_status_page  (map_base),
			_execlist_context(map_base + HW_STATUS_PAGE_SIZE),
			_ppgtt_context   (map_base + HW_STATUS_PAGE_SIZE)
		{ }

		void setup(addr_t     const ring_buffer_start,
		           size_t     const ring_buffer_length,
		           uint64_t   const plm4_addr,
		           Generation const gen)
		{
			auto const map_base = _hw_status_page.base();

			_execlist_context.setup(ring_buffer_start, ring_buffer_length,
			                        EXECLIST_CTX_IH, gen);
			_ppgtt_context.setup(plm4_addr);

			if (false)
				Genode::log(__func__, ":",
				            " map_base:", Genode::Hex(map_base),
				            " ring_buffer_start:", Genode::Hex(ring_buffer_start),
				            " ring_buffer_length:", Genode::Hex(ring_buffer_length),
				            " plm4_addr:", Genode::Hex(plm4_addr, Genode::Hex::PREFIX, Genode::Hex::PAD));

			using C = Execlist_context<RCS_RING_BASE>;

			_execlist_context.write_offset<C::Bb_per_ctx_ptr_mmio>(RCS_RING_BASE);
			{
				using R = C::Bb_per_ctx_ptr_value;
				typename R::access_t v = _execlist_context.read<R>();
				R::Address::set(v, 0);
				R::Valid::set(v, 0);
				_execlist_context.write<R>(v);
			}

			_execlist_context.write_offset<C::Rcs_indirect_ctx_mmio>(RCS_RING_BASE);
			{
				using R = C::Rcs_indirect_ctx_value;
				typename R::access_t v = _execlist_context.read<R>();
				R::Address::set(v, 0);
				R::Size::set(v, 0);
				_execlist_context.write<R>(v);
			}

			_execlist_context.write_offset<C::Rcs_indirect_ctx_offset_mmio>(RCS_RING_BASE);
			{
				using R = C::Rcs_indirect_ctx_offset_value;
				typename R::access_t v = _execlist_context.read<R>();
				R::Offset::set(v, 0);
				_execlist_context.write<R>(v);
			}

			using P = Ppgtt_context<RCS_RING_BASE>;

			_ppgtt_context.write<P::Load_immediate_header>(PPGTT_CTX_IH);
			_ppgtt_context.write<P::Load_immediate_header_2>(PPGTT_CTX_IH_2);
		}

		size_t head_offset()
		{
			return _execlist_context.head_offset();
		}

		void tail_offset(addr_t offset)
		{
			_execlist_context.tail_offset(offset);
		}

		addr_t tail_offset()
		{
			return _execlist_context.tail_offset();
		}

		/*********************
		 ** Debug interface **
		 *********************/

		void dump()
		{
			using namespace Genode;

			log("Rcs_context");
			log("  HW status page:   ", Hex(_hw_status_page.base(), Hex::PREFIX, Hex::PAD));
			log("  Execlist_context: ", Hex(_execlist_context.base(), Hex::PREFIX, Hex::PAD));
			log("  Ppgtt_context:    ", Hex(_ppgtt_context.base(), Hex::PREFIX, Hex::PAD));

			_execlist_context.dump();

			using C = Execlist_context<RCS_RING_BASE>;

			log("  Bb_per_ctx_ptr:          ", Hex(_execlist_context.read<C::Bb_per_ctx_ptr_value>(), Hex::PREFIX, Hex::PAD));
			log("  Rcs_indirect_ctx:        ", Hex(_execlist_context.read<C::Rcs_indirect_ctx_value>(), Hex::PREFIX, Hex::PAD));
			log("  Rcs_indirect_ctx_offset: ", Hex(_execlist_context.read<C::Rcs_indirect_ctx_offset_value>(), Hex::PREFIX, Hex::PAD));

			_ppgtt_context.dump();
		}

		void dump_execlist_context()
		{
			using namespace Genode;

			log("Execlist_context");
			uint32_t const *p = (uint32_t const *)_execlist_context.base();
			for (uint32_t i = 0; i < EXECLIST_CTX_SIZE / sizeof(uint32_t); i += 8) {
				log("  ", Hex(i, Hex::PREFIX, Hex::PAD), "  ",
				    Hex(p[i  ], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+1], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+2], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+3], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+4], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+5], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+6], Hex::PREFIX, Hex::PAD), " ",
				    Hex(p[i+7], Hex::PREFIX, Hex::PAD));
			}
		}
};

#endif /* _LOGICAL_RING_CONTEXT_H_ */
