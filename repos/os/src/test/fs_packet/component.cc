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
	Genode::Env &env;

	Attached_rom_dataspace config_rom { env, "config" };

	Heap heap { env.pd(), env.rm() };
	Allocator_avl avl_alloc { &heap };
	File_system::Connection fs { env, avl_alloc, "", "/", false, 4<<10 };
	File_system::Session::Tx::Source &pkt_tx { *fs.tx() };

	Dir_handle dir_handle { fs.dir("/", false) };
	File_handle file_handle { fs.file(dir_handle, "test", READ_ONLY, false) };

	Signal_handler<Main> ack_handler { env.ep(), *this, &Main::handle_ack };

	int pkt_count = config_rom.xml().attribute_value("count", 1U << 10);

	void handle_ack()
	{
		while (pkt_tx.ack_avail()) {
			auto pkt = pkt_tx.get_acked_packet();
			--pkt_count;
			if (pkt_count < 0) {
				log("--- test complete ---");
				env.parent().exit(0);
				sleep_forever();
			}

			if (!(pkt_count % 10))
				Genode::log(pkt_count, " packets remain");

			pkt_tx.submit_packet(pkt);
		}
	}

	Main(Genode::Env &env) : env(env)
	{
		fs.sigh_ack_avail(ack_handler);

		/**********************
		 ** Stuff the buffer **
		 **********************/

		size_t const pkt_size =
			pkt_tx.bulk_buffer_size() / File_system::Session::TX_QUEUE_SIZE;
		for (size_t i = 0; i < pkt_tx.bulk_buffer_size(); i += pkt_size) {
			File_system::Packet_descriptor pkt(
				pkt_tx.alloc_packet(pkt_size), file_handle,
				File_system::Packet_descriptor::READ, pkt_size, 0);
			pkt_tx.submit_packet(pkt);
		}
	}
};

void Component::construct(Genode::Env &env)
{
	static Fs_packet::Main test(env);
	Genode::log("--- submiting ", test.pkt_count, " packets ---");
}
