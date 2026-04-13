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

/* local includes */
#include <drivers.h>

class Sculpt::Drivers::Instance : Noncopyable
{
	private:

		using Action = Drivers::Action;

		Env        &_env;
		Allocator  &_alloc;
		Action     &_action;

		Board_info _board_info { };
		Resumed    _resumed    { };

		Rom_handler<Instance> _devices {
			_env, "report -> platform/devices", *this, &Instance::_handle_devices };

		void _handle_devices(Node const &devices)
		{
			_board_info.detected = Board_info::Detected::from_node(devices);
			_board_info.used = Board_info::Used::from_node(devices);

			_resumed = { devices.attribute_value("resumed", 0u) };

			_action.handle_device_plug_unplug();
		}

		void _handle_devices()
		{
			_devices.with_node([&] (Node const &devices) {
				_handle_devices(devices); });
		}

	public:

		Instance(Env &env, Allocator &alloc, Action &action)
		:
			_env(env), _alloc(alloc), _action(action)
		{ }

		void update_soc(Board_info::Soc soc)
		{
			_board_info.soc = soc;
			_handle_devices();
		}

		void update_options(Board_info::Options const options)
		{
			if (options != _board_info.options) {
				_board_info.options = options;
				_handle_devices();
			}
		}

		void with(With_board_info::Ft const &fn) const { fn(_board_info); }

		bool ready_for_suspend() const { return !_board_info.used.any(); }

		Resumed resumed() const { return _resumed; }
};


using namespace Sculpt;


Drivers::Instance &Drivers::_construct_instance(auto &&... args)
{
	static bool called_once;
	if (called_once)
		error("unexpected attempt to construct multiple 'Drivers' instances");
	called_once = true;

	static Drivers::Instance instance { args... };
	return instance;
}


Sculpt::Drivers::Drivers(Env &env, Allocator &alloc, Action &action)
:
	_instance(_construct_instance(env, alloc, action))
{ }

void Drivers::_with(With_board_info::Ft const &fn) const { _instance.with(fn); }

void Drivers::update_soc    (Board_info::Soc     soc) { _instance.update_soc(soc); }
void Drivers::update_options(Board_info::Options opt) { _instance.update_options(opt); }

bool Drivers::ready_for_suspend() const { return _instance.ready_for_suspend(); };

Drivers::Resumed Drivers::resumed() const { return _instance.resumed(); };
