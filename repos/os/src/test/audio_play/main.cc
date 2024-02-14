/*
 * \brief  Play audio sample (stereo, interleaved, 32-bit floating point)
 * \author Norman Feske
 * \date   2024-02-14
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <play_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <os/vfs.h>

namespace Audio_play {

	using namespace Genode;

	struct Main;
}


struct Audio_play::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Attached_rom_dataspace _config { _env, "config" };

	Vfs::Global_file_system_factory _fs_factory { _heap };
	Vfs::Simple_env _vfs_env { _env, _heap, _config.xml().sub_node("vfs") };
	Directory _root_dir { _vfs_env };

	Directory::Path const _sample_path =
		_config.xml().attribute_value("sample_path", Directory::Path());

	File_content const _sample_data {
		_heap, _root_dir, _sample_path, { _env.pd().avail_ram().value } };

	Play::Connection _left  { _env, "left"  },
	                 _right { _env, "right" };

	Play::Time_window _time_window { };

	unsigned _pos = 0;

	unsigned const _period_ms = 5,
	               _sample_rate_hz = 44100,
	               _frames_per_period = (_period_ms*_sample_rate_hz)/1000;

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	struct Frame { float left, right; } __attribute__((packed));

	static_assert(sizeof(Frame) == 8);

	void _for_each_frame_of_period(auto const &fn) const
	{
		_sample_data.bytes([&] (char const * const ptr, size_t const num_bytes) {
			if (num_bytes >= sizeof(Frame))
				for (unsigned i = 0; i < _frames_per_period; i++)
					fn(*(Frame const *)(ptr + ((_pos + i*sizeof(Frame)) % num_bytes))); });
	}

	void _handle_timer()
	{
		_time_window = _left.schedule_and_enqueue(_time_window, { _period_ms*1000 },
			[&] (auto &submit) {
				_for_each_frame_of_period([&] (Frame const frame) {
					submit(frame.left); }); });

		_right.enqueue(_time_window,
			[&] (auto &submit) {
				_for_each_frame_of_period([&] (Frame const frame) {
					submit(frame.right); }); });

		_pos = unsigned(_pos + _frames_per_period*sizeof(Frame));
	}

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(_period_ms*1000);
	}
};


void Component::construct(Genode::Env &env) { static Audio_play::Main main(env); }
