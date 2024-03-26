/*
 * \brief  Common types used within the Sculpt manager
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <util/list_model.h>
#include <base/env.h>
#include <base/attached_rom_dataspace.h>
#include <platform_session/platform_session.h>
#include <gui_session/gui_session.h>
#include <usb_session/usb_session.h>
#include <log_session/log_session.h>
#include <rm_session/rm_session.h>
#include <timer_session/timer_session.h>
#include <file_system_session/file_system_session.h>
#include <report_session/report_session.h>
#include <block_session/block_session.h>
#include <terminal_session/terminal_session.h>
#include <rom_session/rom_session.h>
#include <rm_session/rm_session.h>
#include <nic_session/nic_session.h>
#include <rtc_session/rtc_session.h>
#include <trace_session/trace_session.h>
#include <io_mem_session/io_mem_session.h>
#include <io_port_session/io_port_session.h>

namespace Sculpt {

	using namespace Genode;

	typedef String<64>  Rom_name;
	typedef String<128> Path;
	typedef String<36>  Start_name;

	typedef Gui::Point Point;
	typedef Gui::Rect  Rect;
	typedef Gui::Area  Area;

	enum Writeable { WRITEABLE, READ_ONLY };

	/*
	 * CPU priorities used within the runtime subsystem
	 */
	enum class Priority {
		BACKGROUND   = -3,
		DEFAULT      = -2,
		NETWORK      = DEFAULT,
		STORAGE      = DEFAULT,
		MULTIMEDIA   = -1,
		DRIVER       = 0,
		LEITZENTRALE = 0         /* only for latency-critical drivers */
	};

	/**
	 * Argument type for controlling the verification of downloads
	 */
	struct Verify { bool value; };

	/**
	 * Utility for passing lambda arguments to non-template functions
	 */
	template <typename... ARGS>
	struct With
	{
		struct Callback : Interface
		{
			virtual void operator () (ARGS &&...) const = 0;
		};

		template <typename FN>
		struct Fn : Callback
		{
			FN const &_fn;
			Fn(FN const &fn) : _fn(fn) { };
			void operator () (ARGS &&... args) const override { _fn(args...); }
		};
	};

	struct Progress { bool progress; };
}

#endif /* _TYPES_H_ */
