/*
 * \brief  ROM-file to block-session client test implementation
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2010-07-07
 *
 * The test program compares the values delivered by the block-service,
 * with the original rom-file.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/heap.h>
#include <base/component.h>
#include <block_session/connection.h>
#include <base/attached_rom_dataspace.h>

using namespace Genode;

struct Main
{
	static constexpr Block::sector_t REQ_PARALLEL = 10;

	using  File_name           = String<64>;
	using  Packet_descriptor   = Block::Packet_descriptor;
	struct Files_differ        : Exception { };
	struct Device_not_readable : Exception { };
	struct Read_request_failed : Exception { };

	Env                    &env;
	Attached_rom_dataspace  config      { env, "config" };
	File_name               file_name   { config.xml().attribute_value("file", File_name()) };
	Heap                    heap        { env.ram(), env.rm() };
	Allocator_avl           block_alloc { &heap };
	Block::Connection<>     block       { env, &block_alloc };
	Attached_rom_dataspace  rom         { env, file_name.string() };

	Main(Env &env) : env(env)
	{
		log("--- ROM Block test ---");

		Block::Session::Tx::Source &src = *block.tx();
		Block::Session::Info const info = block.info();

		log("We have ", info.block_count, " blocks with a "
		    "size of ", info.block_size, " bytes");
		for (size_t i = 0; i < info.block_count; i += REQ_PARALLEL) {

			size_t const cnt = (info.block_count - i > REQ_PARALLEL)
			                 ? REQ_PARALLEL : info.block_count - i;

			Packet_descriptor pkt(block.alloc_packet(cnt * info.block_size),
			                      Packet_descriptor::READ, i, cnt);

			log("Check blocks ", i, "..", i + cnt - 1);
			src.submit_packet(pkt);
			pkt = src.get_acked_packet();
			if (!pkt.succeeded()) {
				throw Read_request_failed(); }

			char const *rom_src = rom.local_addr<char>() + i * info.block_size;
			if (strcmp(rom_src, src.packet_content(pkt), rom.size())) {
				throw Files_differ(); }

			src.release_packet(pkt);
		}
		log("--- ROM Block test finished ---");
	}
};

void Component::construct(Env &env) { static Main main(env); }
