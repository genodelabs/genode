/*
 * \brief  Sculpt dynamic drivers management
 * \author Norman Feske
 * \date   2024-03-25
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS_H_
#define _DRIVERS_H_

/* local includes */
#include <xml.h>
#include <util/callable.h>
#include <model/child_state.h>
#include <model/board_info.h>

namespace Sculpt { struct Drivers; }


class Sculpt::Drivers : Noncopyable
{
	public:

		struct Action : Interface
		{
			virtual void handle_device_plug_unplug() = 0;
		};

		/**
		 * Argument type for 'with_storage_devices'
		 */
		struct Storage_devices
		{
			struct Driver
			{
				bool const  present;
				Node const &report;

				bool enumerated() const { return !present || !report.has_type("empty"); }
			};

			Driver const &usb, &ahci, &nvme, &mmc;

			bool all_enumerated() const { return usb.enumerated() && ahci.enumerated()
			                                 && nvme.enumerated() &&  mmc.enumerated(); }
		};

	private:

		struct Instance;

		Instance &_instance;

		static Instance &_construct_instance(auto &&...);

		using With_board_info = Callable<void, Board_info const &>;
		using With_node       = Callable<void, Node const &>;

		void _with(With_board_info::Ft const &) const;

	public:

		Drivers(Env &, Allocator &, Action &);

		void update_soc(Board_info::Soc);
		void update_options(Board_info::Options);

		void with_board_info(auto const &fn) const { _with(With_board_info::Fn { fn }); }

		/* true if hardware is suspend/resume capable */
		bool suspend_supported() const;

		/* true once 'Board_info::Option::suspending' phase is compete */
		bool ready_for_suspend() const;

		struct Resumed { unsigned count; };

		Resumed resumed() const;
};

#endif /* _DRIVERS_H_ */
