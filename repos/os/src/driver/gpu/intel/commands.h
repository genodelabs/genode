/*
 * \brief  Broadwell MI commands
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMMANDS_H_
#define _COMMANDS_H_

/* Genode includes */
#include <util/mmio.h>
#include <util/string.h>

/* local includes */
#include <types.h>


namespace Igd {

	struct Cmd_header;
	struct Mi_cmd;
	struct Mi_noop;
	struct Mi_arb_check;
	struct Mi_arb_on_off;
	struct Mi_user_interrupt;
	struct Mi_batch_buffer_start;
	struct Mi_semaphore_wait;
	struct Pipe_control;

	void cmd_dump(uint32_t cmd, uint32_t index = 0);
}


/*
 * IHD-OS-BDW-Vol 6-11.15 p. 2
 */
struct Igd::Cmd_header : Genode::Register<32>
{
	struct Cmd_type : Bitfield<29, 3>
	{
		enum {
			MI_COMMAND = 0b000,
			MI_BCS     = 0b010,
			MI_RCS     = 0b011,
		};
	};

	struct Cmd_subtype : Bitfield<27, 2> { };
	struct Cmd_opcode  : Bitfield<24, 3> { };

	/*
	 * Actually bit 23:x seems to be the sub-opcode but opcodes
	 * include bit 23 (see p. 5).
	 */
	struct Mi_cmd_opcode : Bitfield<23, 6>
	{
		enum Opcode {
			MI_NOOP                 = 0x00,
			MI_USER_INTERRUPT       = 0x02,
			MI_WAIT_FOR_EVENT       = 0x03,
			MI_FLUSH                = 0x04,
			MI_ARB_CHECK            = 0x05,
			MI_REPORT_HEAD          = 0x07,
			MI_ARB_ON_OFF           = 0x08,
			MI_BATCH_BUFFER_END     = 0x0A,
			MI_SUSPEND_FLUSH        = 0x0B,
			MI_SET_APPID            = 0x0E,
			MI_OVERLAY_FLIP         = 0x11,
			MI_LOAD_SCAN_LINES_INCL = 0x12,
			MI_DISPLAY_FLIP         = 0x14,
			MI_DISPLAY_FLIP_I915    = 0x14,
			MI_SEMAPHORE_MBOX       = 0x16,
			MI_SET_CONTEXT          = 0x18,
			MI_SEMAPHORE_SIGNAL     = 0x1b,
			MI_SEMAPHORE_WAIT       = 0x1c,
			MI_STORE_DWORD_IMM      = 0x20,
			MI_STORE_DWORD_INDEX    = 0x21,
			MI_LOAD_REGISTER_IMM    = 0x22,
			MI_STORE_REGISTER_MEM   = 0x24,
			MI_FLUSH_DW             = 0x26,
			MI_LOAD_REGISTER_MEM    = 0x29,
			MI_BATCH_BUFFER         = 0x30,
			MI_BATCH_BUFFER_START   = 0x31,
		};
	};

	typename Cmd_header::access_t value;

	Cmd_header() : value(0) { }

	Cmd_header(Igd::uint32_t value) : value(value) { }
};


struct Igd::Mi_cmd : Cmd_header
{
	Mi_cmd(Mi_cmd_opcode::Opcode opcode)
	{
		Cmd_type::set(value, Cmd_type::MI_COMMAND);
		Mi_cmd_opcode::set(value, opcode);
	};
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 870
 */
struct Igd::Mi_noop : Mi_cmd
{
	Mi_noop()
	:
	  Mi_cmd(Mi_cmd_opcode::MI_NOOP)
	{ }
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 948 ff.
 */
struct Igd::Mi_user_interrupt : Mi_cmd
{

	Mi_user_interrupt()
	:
	  Mi_cmd(Mi_cmd_opcode::MI_USER_INTERRUPT)
	{ }
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 777 ff.
 */
struct Igd::Mi_arb_check : Mi_cmd
{
	Mi_arb_check()
	:
	  Mi_cmd(Mi_cmd_opcode::MI_ARB_CHECK)
	{ }
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 781 ff.
 */
struct Igd::Mi_arb_on_off : Mi_cmd
{
	struct Enable : Bitfield<0, 1> { };

	Mi_arb_on_off(bool enable)
	:
	  Mi_cmd(Mi_cmd_opcode::MI_ARB_ON_OFF)
	{
		Enable::set(value, enable ? 1 : 0);
	}
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 793 ff.
 */
struct Igd::Mi_batch_buffer_start : Mi_cmd
{
	struct Address_space_indicator : Bitfield<8, 1>
	{
		enum { GTT = 0b0, PPGTT = 0b1, };
	};

	struct Dword_length : Bitfield<0, 8> { };

	Mi_batch_buffer_start()
	:
	  Mi_cmd(Mi_cmd_opcode::MI_BATCH_BUFFER_START)
	{
		Address_space_indicator::set(Cmd_header::value, Address_space_indicator::PPGTT);

		Dword_length::set(Cmd_header::value, 1);
	}
};


/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 888 ff.
 *
 * Note: Legnth 2 on GEN8+ and 3 on GEN12+
 */
struct Igd::Mi_semaphore_wait : Mi_cmd
{

	struct Compare_operation : Bitfield<12, 3>
	{
		enum { SAD_EQUAL_SDD = 0x4 };
	};

	struct Wait_mode : Bitfield<15, 1>
	{
		enum { SIGNAL = 0b0, POLL = 0b1};
	};

	struct Memory_type : Bitfield<22, 1>
	{
		enum { PPGTT = 0b0, GGTT = 0b1};
	};

	/* number of words - 2 */
	struct Dword_length : Bitfield<0, 8> { };

	Mi_semaphore_wait()
	:
	  Mi_cmd(Mi_cmd_opcode::MI_SEMAPHORE_WAIT)
	{
		Memory_type::set(Cmd_header::value, Memory_type::GGTT);
		Wait_mode::set(Cmd_header::value, Wait_mode::POLL);
		/* wait for address data equal data word */
		Compare_operation::set(Cmd_header::value, Compare_operation::SAD_EQUAL_SDD);
	}

	void dword_length(unsigned const value)
	{
		Dword_length::set(Cmd_header::value, value);
	}
};

/*
 * IHD-OS-BDW-Vol 2a-11.15 p. 983 ff.
 */
struct Igd::Pipe_control : Cmd_header
{
	struct Dword_length : Bitfield<0, 8> { };

	enum {
		GFX_PIPE_LINE = 0b11,
		PIPE_CONTROL  = 0b10,
	};

	enum {
		FLUSH_L3                     = (1 << 27),
		GLOBAL_GTT_IVB               = (1 << 24),
		MMIO_WRITE                   = (1 << 23),
		STORE_DATA_INDEX             = (1 << 21),
		CS_STALL                     = (1 << 20),
		TLB_INVALIDATE               = (1 << 18),
		MEDIA_STATE_CLEAR            = (1 << 16),
		QW_WRITE                     = (1 << 14),
		POST_SYNC_OP_MASK            = (3 << 14),
		DEPTH_STALL                  = (1 << 13),
		WRITE_FLUSH                  = (1 << 12),
		RENDER_TARGET_CACHE_FLUSH    = (1 << 12),
		INSTRUCTION_CACHE_INVALIDATE = (1 << 11),
		TEXTURE_CACHE_INVALIDATE     = (1 << 10),
		INDIRECT_STATE_DISABLE       = (1 <<  9),
		NOTIFY                       = (1 <<  8),
		FLUSH_ENABLE                 = (1 <<  7),
		DC_FLUSH_ENABLE              = (1 <<  5),
		VF_CACHE_INVALIDATE          = (1 <<  4),
		CONST_CACHE_INVALIDATE       = (1 <<  3),
		STATE_CACHE_INVALIDATE       = (1 <<  2),
		STALL_AT_SCOREBOARD          = (1 <<  1),
		DEPTH_CACHE_FLUSH            = (1 <<  0),
	};

	Pipe_control(Genode::uint8_t length)
	{
		Cmd_header::Cmd_type::set(Cmd_header::value,
		                          Cmd_header::Cmd_type::MI_RCS);
		Cmd_header::Cmd_subtype::set(Cmd_header::value, GFX_PIPE_LINE);
		Cmd_header::Cmd_opcode::set(Cmd_header::value, PIPE_CONTROL);

		Dword_length::set(Cmd_header::value, (length-2));
	}
};

#endif /* _COMMANDS_H_ */
