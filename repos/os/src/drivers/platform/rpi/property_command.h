/*
 * \brief  Command definitions for the property mbox channel
 * \author Norman Feske
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PROPERTY_COMMAND_H_
#define _PROPERTY_COMMAND_H_

/* Genode includes */
#include <base/stdint.h>

namespace Property_command {

	using namespace Genode;

	struct Get_power_state
	{
		static uint32_t opcode() { return 0x00020001; };

		struct Request
		{
			uint32_t const device_id;

			Request(uint32_t device_id) : device_id(device_id) { }
		};

		struct Response
		{
			uint32_t const device_id = 0;
			uint32_t const state     = 0;
		};
	};

	struct Set_power_state
	{
		static uint32_t opcode() { return 0x00028001; };

		struct Request
		{
			uint32_t const device_id;
			uint32_t const state;

			Request(uint32_t device_id, bool enable, bool wait)
			:
				device_id(device_id),
				state(enable | (wait << 1))
			{ }
		};

		struct Response
		{
			uint32_t const device_id = 0;
			uint32_t const state     = 0;
		};
	};

	struct Get_clock_rate
	{
		static uint32_t opcode() { return 0x00030002; };

		struct Request
		{
			uint32_t const clock_id;

			Request(uint32_t clock_id) : clock_id(clock_id) { }
		};

		struct Response
		{
			uint32_t const clock_id = 0;
			uint32_t const hz       = 0;
		};
	};

	struct Allocate_buffer
	{
		static uint32_t opcode() { return 0x00040001; };

		struct Request
		{
			uint32_t const alignment = 0x100;
		};

		struct Response
		{
			uint32_t const address = 0;
			uint32_t const size    = 0;
		};
	};


	struct Release_buffer
	{
		static uint32_t opcode() { return 0x00048001; };
	};


	struct Get_physical_w_h
	{
		static uint32_t opcode() { return 0x00040003; };

		struct Response
		{
			uint32_t const width  = 0;
			uint32_t const height = 0;
		};
	};


	struct Set_physical_w_h
	{
		static uint32_t opcode() { return 0x00048003; };

		struct Request
		{
			uint32_t const width;
			uint32_t const height;

			Request(uint32_t width, uint32_t height)
			: width(width), height(height) { }
		};
	};
}

#endif /* _PROPERTY_COMMAND_H_ */
