/*
 * \brief  File_system packet processing test
 * \author Emery Hemingway
 * \date   2018-07-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#include <file_system_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <base/component.h>
#include <base/sleep.h>

namespace Fs_packet {
	using namespace Genode;
	using namespace File_system;
	struct Main;
};


struct Fs_packet::Main
{
	Genode::Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	int _packet_count = _config.xml().attribute_value("count", 1U << 10);

	Heap                    _heap { _env.pd(), _env.rm() };
	Allocator_avl           _avl_alloc { &_heap };
	File_system::Connection _fs { _env, _avl_alloc, "", "/", false, 4<<10 };

	File_system::Session::Tx::Source &_tx { *_fs.tx() };

	Dir_handle  _dir_handle  { _fs.dir("/", false) };
	File_handle _file_handle { _fs.file(_dir_handle, "test", READ_ONLY, false) };

	Signal_handler<Main> _signal_handler { _env.ep(), *this, &Main::_handle_signal };

	void _handle_signal()
	{
		while (_tx.ack_avail()) {
			auto packet = _tx.get_acked_packet();
			--_packet_count;
			if (_packet_count < 0) {
				log("--- test complete ---");
				_env.parent().exit(0);
				sleep_forever();
			}

			if (!(_packet_count % 10))
				log(_packet_count, " packets remain");

			_tx.submit_packet(packet);
		}
	}

	Main(Genode::Env &env) : _env(env)
	{
		_fs.sigh(_signal_handler);

		/**********************
		 ** Stuff the buffer **
		 **********************/

		size_t const packet_size =
			_tx.bulk_buffer_size() / File_system::Session::TX_QUEUE_SIZE;

		for (size_t i = 0; i < _tx.bulk_buffer_size(); i += packet_size) {
			File_system::Packet_descriptor packet(
				_tx.alloc_packet(packet_size), _file_handle,
				File_system::Packet_descriptor::READ, packet_size, 0);
			_tx.submit_packet(packet);
		}

		log("--- submiting ", _packet_count, " packets ---");
	}
};


void Component::construct(Genode::Env &env)
{
	static Fs_packet::Main test(env);
}
