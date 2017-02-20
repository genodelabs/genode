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
	enum { REQ_PARALLEL = 10 };

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
	Block::Connection       block       { env, &block_alloc };
	Attached_rom_dataspace  rom         { env, file_name.string() };

	Main(Env &env) : env(env)
	{
		log("--- ROM Block test ---");

		Block::Session::Tx::Source &src = *block.tx();
		size_t                      blk_sz;
		Block::sector_t             blk_cnt;
		Block::Session::Operations  ops;

		block.info(&blk_cnt, &blk_sz, &ops);
		if (!ops.supported(Packet_descriptor::READ)) {
			throw Device_not_readable(); }

		log("We have ", blk_cnt, " blocks with a size of ", blk_sz, " bytes");
		for (size_t i = 0; i < blk_cnt; i += REQ_PARALLEL) {
			size_t cnt = (blk_cnt - i > REQ_PARALLEL) ? REQ_PARALLEL : blk_cnt - i;
			Packet_descriptor pkt(src.alloc_packet(cnt * blk_sz),
			                      Packet_descriptor::READ, i, cnt);

			log("Check blocks ", i, "..", i + cnt - 1);
			src.submit_packet(pkt);
			pkt = src.get_acked_packet();
			if (!pkt.succeeded()) {
				throw Read_request_failed(); }

			char const *rom_src = rom.local_addr<char>() + i * blk_sz;
			if (strcmp(rom_src, src.packet_content(pkt), rom.size())) {
				throw Files_differ(); }

			src.release_packet(pkt);
		}
		log("--- ROM Block test finished ---");
	}
};

void Component::construct(Env &env) { static Main main(env); }
